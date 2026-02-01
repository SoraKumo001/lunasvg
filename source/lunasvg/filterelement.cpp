#include "filterelement.h"
#include "renderstate.h"
#include "layoutstate.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lunasvg {

// --- Color Space Conversion Helpers ---
static const auto srgbToLinearTable = []() {
    std::array<float, 256> table;
    for(int i = 0; i < 256; ++i) {
        float c = i / 255.f;
        table[i] = (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    return table;
}();

static inline float toLinear(float c) {
    return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static inline float toSRGB(float c) {
    return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * std::pow(c, 1.f / 2.4f) - 0.055f);
}

static inline uint8_t toByte(float v) {
    return (uint8_t)std::round(std::clamp(v, 0.f, 1.f) * 255.f);
}

// --- FilterImage Implementation ---
FilterImage::FilterImage(int w, int h) : m_width(w), m_height(h), m_pixels(w * h, {0,0,0,0}) {}

std::shared_ptr<FilterImage> FilterImage::fromCanvas(const Canvas& canvas) {
    int w = canvas.width(), h = canvas.height();
    auto image = std::make_shared<FilterImage>(w, h);
    Bitmap bitmap(plutovg_surface_reference(canvas.surface()));
    const uint8_t* data = bitmap.data();
    int stride = bitmap.stride();
    FilterPixel* dst = image->data();
    for(int y = 0; y < h; ++y) {
        const uint8_t* src_row = data + y * stride;
        FilterPixel* dst_row = dst + y * w;
        for(int x = 0; x < w; ++x) {
            uint8_t alpha = src_row[x*4 + 3];
            if(alpha == 255) {
                dst_row[x].r = srgbToLinearTable[src_row[x*4 + 2]];
                dst_row[x].g = srgbToLinearTable[src_row[x*4 + 1]];
                dst_row[x].b = srgbToLinearTable[src_row[x*4 + 0]];
                dst_row[x].a = 1.0f;
            } else if(alpha > 0) {
                float a = alpha / 255.f;
                float invA = 1.f / a;
                dst_row[x].r = toLinear((src_row[x*4 + 2] / 255.f) * invA) * a;
                dst_row[x].g = toLinear((src_row[x*4 + 1] / 255.f) * invA) * a;
                dst_row[x].b = toLinear((src_row[x*4 + 0] / 255.f) * invA) * a;
                dst_row[x].a = a;
            }
        }
    }
    return image;
}

std::shared_ptr<Canvas> FilterImage::toCanvas(const Rect& extents) const {
    auto canvas = Canvas::create(extents);
    Bitmap bitmap(plutovg_surface_reference(canvas->surface()));
    uint8_t* data = bitmap.data();
    int stride = bitmap.stride();
    const FilterPixel* src = this->data();
    for(int y = 0; y < m_height; ++y) {
        uint8_t* dst_row = data + y * stride;
        const FilterPixel* src_row = src + y * m_width;
        for(int x = 0; x < m_width; ++x) {
            const auto& p = src_row[x];
            float a = p.a;
            if(a >= 1.0f) {
                dst_row[x*4 + 3] = 255;
                dst_row[x*4 + 2] = toByte(toSRGB(std::clamp(p.r, 0.f, 1.f)));
                dst_row[x*4 + 1] = toByte(toSRGB(std::clamp(p.g, 0.f, 1.f)));
                dst_row[x*4 + 0] = toByte(toSRGB(std::clamp(p.b, 0.f, 1.f)));
            } else if(a > 0.0001f) {
                float invA = 1.f / a;
                dst_row[x*4 + 3] = toByte(a);
                dst_row[x*4 + 2] = toByte(toSRGB(std::clamp(p.r * invA, 0.f, 1.f)) * a);
                dst_row[x*4 + 1] = toByte(toSRGB(std::clamp(p.g * invA, 0.f, 1.f)) * a);
                dst_row[x*4 + 0] = toByte(toSRGB(std::clamp(p.b * invA, 0.f, 1.f)) * a);
            } else {
                std::memset(&dst_row[x*4], 0, 4);
            }
        }
    }
    return canvas;
}

// --- FilterContext Implementation ---
FilterContext::FilterContext(const SVGFilterElement* filter, const SVGElement* element, const SVGRenderState& state, const Canvas& sourceGraphic)
    : filter(filter), element(element), state(state)
{
    this->sourceGraphic = FilterImage::fromCanvas(sourceGraphic);
    int w = sourceGraphic.width(), h = sourceGraphic.height();
    this->sourceAlpha = std::make_shared<FilterImage>(w, h);
    const FilterPixel* src = this->sourceGraphic->data();
    FilterPixel* dst = this->sourceAlpha->data();
    for(int i = 0; i < w * h; ++i) dst[i] = {0, 0, 0, src[i].a};      
    results.emplace("SourceGraphic", this->sourceGraphic);
    results.emplace("SourceAlpha", this->sourceAlpha);
    lastResult = this->sourceGraphic;
}

std::shared_ptr<FilterImage> FilterContext::getInput(const std::string& in) {
    if(in.empty()) return lastResult;
    auto it = results.find(in);
    return (it != results.end()) ? it->second : nullptr;
}

void FilterContext::addResult(const std::string& result, std::shared_ptr<FilterImage> image) {
    lastResult = image;
    if(!result.empty()) results[result] = std::move(image);
}

// --- Box Blur Implementation ---
static void boxBlur(const FilterPixel* src, FilterPixel* dst, int w, int h, int r, bool horizontal) {
    if (r <= 0) return;
    float iarr = 1.f / (r + r + 1);
    if (horizontal) {
        for(int y = 0; y < h; y++) {
            const FilterPixel* src_row = src + y * w;
            FilterPixel* dst_row = dst + y * w;
            FilterPixel val = {0, 0, 0, 0};
            FilterPixel fv = src_row[0];
            FilterPixel lv = src_row[w - 1];
            for(int j = 0; j < r; j++) { val.r += fv.r; val.g += fv.g; val.b += fv.b; val.a += fv.a; }
            for(int j = 0; j <= r; j++) { const auto& p = src_row[j]; val.r += p.r; val.g += p.g; val.b += p.b; val.a += p.a; }
            for(int x = 0; x < w; x++) {
                dst_row[x] = {val.r * iarr, val.g * iarr, val.b * iarr, val.a * iarr};
                int ri = x + r + 1;
                const auto& p_ri = (ri < w) ? src_row[ri] : lv;
                int li = x - r;
                const auto& p_li = (li >= 0) ? src_row[li] : fv;
                val.r += p_ri.r - p_li.r; val.g += p_ri.g - p_li.g; val.b += p_ri.b - p_li.b; val.a += p_ri.a - p_li.a;
            }
        }
    } else {
        for(int x = 0; x < w; x++) {
            FilterPixel val = {0, 0, 0, 0};
            FilterPixel fv = src[x];
            FilterPixel lv = src[x + (h - 1) * w];
            for(int j = 0; j < r; j++) { val.r += fv.r; val.g += fv.g; val.b += fv.b; val.a += fv.a; }
            for(int j = 0; j <= r; j++) { const auto& p = src[x + j * w]; val.r += p.r; val.g += p.g; val.b += p.b; val.a += p.a; }
            for(int y = 0; y < h; y++) {
                dst[x + y * w] = {val.r * iarr, val.g * iarr, val.b * iarr, val.a * iarr};
                int ri = y + r + 1;
                const auto& p_ri = (ri < h) ? src[x + ri * w] : lv;
                int li = y - r;
                const auto& p_li = (li >= 0) ? src[x + li * w] : fv;
                val.r += p_ri.r - p_li.r; val.g += p_ri.g - p_li.g; val.b += p_ri.b - p_li.b; val.a += p_ri.a - p_li.a;
            }
        }
    }
}

void SVGFeGaussianBlurElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()); if(!input) return;
    int w = input->width(), h = input->height();
    float stdDevX = 0, stdDevY = 0;
    if(!m_stdDeviation.values().empty()) {
        stdDevX = m_stdDeviation.values()[0];
        stdDevY = m_stdDeviation.values().size() > 1 ? m_stdDeviation.values()[1] : stdDevX;
    }
    
    auto current = std::make_shared<FilterImage>(w, h);
    std::memcpy(current->data(), input->data(), w * h * sizeof(FilterPixel));

    if(stdDevX > 0 || stdDevY > 0) {
        auto next = std::make_shared<FilterImage>(w, h);
        int rx = (int)std::floor(stdDevX * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        int ry = (int)std::floor(stdDevY * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        for(int i = 0; i < 3; i++) {
            if(rx > 0) { boxBlur(current->data(), next->data(), w, h, rx, true); std::swap(current, next); }
            if(ry > 0) { boxBlur(current->data(), next->data(), w, h, ry, false); std::swap(current, next); }
        }
    }
    context.addResult(this->result().value(), current);
}

void SVGFeOffsetElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()); if(!input) return;
    int w = input->width(), h = input->height(); auto result = std::make_shared<FilterImage>(w, h);
    int ox = (int)std::round(this->dx().value()), oy = (int)std::round(this->dy().value());
    const FilterPixel* src = input->data();
    FilterPixel* dst = result->data();
    for(int y = 0; y < h; y++) {
        int sy = y - oy;
        if(sy < 0 || sy >= h) continue;
        const FilterPixel* src_row = src + sy * w;
        FilterPixel* dst_row = dst + y * w;
        for(int x = 0; x < w; x++) {
            int sx = x - ox;
            if(sx >= 0 && sx < w) dst_row[x] = src_row[sx];      
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeDropShadowElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()); if(!input) return;
    int w = input->width(), h = input->height();
    auto shadow = std::make_shared<FilterImage>(w, h);
    float ra = m_flood_opacity, rr = toLinear(m_flood_color.redF()) * ra, rg = toLinear(m_flood_color.greenF()) * ra, rb = toLinear(m_flood_color.blueF()) * ra;
    const FilterPixel* input_data = input->data();
    FilterPixel* shadow_data = shadow->data();
    for(int i = 0; i < w * h; i++) shadow_data[i] = {rr, rg, rb, input_data[i].a * ra};
    
    float stdDevX = 0, stdDevY = 0;
    if(!m_stdDeviation.values().empty()) { stdDevX = m_stdDeviation.values()[0]; stdDevY = m_stdDeviation.values().size() > 1 ? m_stdDeviation.values()[1] : stdDevX; }
    if(stdDevX > 0 || stdDevY > 0) {
        auto temp = std::make_shared<FilterImage>(w, h);
        int rx = (int)std::floor(stdDevX * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2, ry = (int)std::floor(stdDevY * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        for(int i = 0; i < 3; i++) {
            if(rx > 0) { boxBlur(shadow->data(), temp->data(), w, h, rx, true); std::swap(shadow, temp); }
            if(ry > 0) { boxBlur(shadow->data(), temp->data(), w, h, ry, false); std::swap(shadow, temp); }
        }
    }
    auto result = std::make_shared<FilterImage>(w, h);
    int ox = (int)std::round(this->dx().value()), oy = (int)std::round(this->dy().value());
    FilterPixel* dst = result->data();
    shadow_data = shadow->data();
    for(int y = 0; y < h; y++) {
        int sy = y - oy;
        for(int x = 0; x < w; x++) {
            int sx = x - ox;
            FilterPixel s = (sx >= 0 && sx < w && sy >= 0 && sy < h) ? shadow_data[sy * w + sx] : FilterPixel{0,0,0,0};
            const auto& g = input_data[y * w + x];
            dst[y * w + x] = { g.r + s.r * (1.f - g.a), g.g + s.g * (1.f - g.a), g.b + s.b * (1.f - g.a), g.a + s.a * (1.f - g.a) };
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeMergeElement::render(FilterContext& context) const {
    int w = context.sourceGraphic->width(), h = context.sourceGraphic->height();
    auto result = std::make_shared<FilterImage>(w, h);
    for(const auto& child : children()) {
        if(auto node = toSVGElement(child); node && node->id() == ElementID::FeMergeNode) {
            auto input = context.getInput(static_cast<const SVGFeMergeNodeElement*>(node)->in().value());
            if(input) {
                const FilterPixel* src = input->data();
                FilterPixel* dst = result->data();
                for(int i = 0; i < w * h; i++) {
                    const auto& s = src[i]; auto& d = dst[i];
                    d.r = s.r + d.r * (1.f - s.a); d.g = s.g + d.g * (1.f - s.a); d.b = s.b + d.b * (1.f - s.a); d.a = s.a + d.a * (1.f - s.a);
                }
            }
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeCompositeElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()), input2 = context.getInput(this->m_in2.value());
    if(!input || !input2) return;
    int w = input->width(), h = input->height(); auto result = std::make_shared<FilterImage>(w, h);
    float fk1 = m_k1.value(), fk2 = m_k2.value(), fk3 = m_k3.value(), fk4 = m_k4.value();
    const FilterPixel* src1 = input->data();
    const FilterPixel* src2 = input2->data();
    FilterPixel* dst = result->data();
    FECompositeOperator op = this->_operator().value();
    for(int i = 0; i < w * h; i++) {
        const auto& s1 = src1[i], &s2 = src2[i];
        if(op == FECompositeOperator::Arithmetic) {
            float na = std::clamp(fk1 * s1.a * s2.a + fk2 * s1.a + fk3 * s2.a + fk4, 0.f, 1.f);
            if(na > 0) {
                float invA1 = s1.a > 0 ? 1.f / s1.a : 0, invA2 = s2.a > 0 ? 1.f / s2.a : 0;
                float c1r = s1.r * invA1, c1g = s1.g * invA1, c1b = s1.b * invA1;
                float c2r = s2.r * invA2, c2g = s2.g * invA2, c2b = s2.b * invA2;
                dst[i] = { std::clamp(fk1*c1r*c2r + fk2*c1r + fk3*c2r + fk4, 0.f, 1.f) * na,
                           std::clamp(fk1*c1g*c2g + fk2*c1g + fk3*c2g + fk4, 0.f, 1.f) * na,
                           std::clamp(fk1*c1b*c2b + fk2*c1b + fk3*c2b + fk4, 0.f, 1.f) * na, na };      
            }
        } else {
            float fa = 0, fb = 0;
            switch(op) {
                case FECompositeOperator::Over: fa = 1; fb = 1 - s1.a; break;
                case FECompositeOperator::In: fa = s2.a; fb = 0; break;
                case FECompositeOperator::Out: fa = 1 - s2.a; fb = 0; break;
                case FECompositeOperator::Atop: fa = s2.a; fb = 1 - s1.a; break;
                case FECompositeOperator::Xor: fa = 1 - s2.a; fb = 1 - s1.a; break;
                default: break;
            }
            dst[i] = { s1.r * fa + s2.r * fb, s1.g * fa + s2.g * fb, s1.b * fa + s2.b * fb, s1.a * fa + s2.a * fb };
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeColorMatrixElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()); if(!input) return;
    int w = input->width(), h = input->height(); auto result = std::make_shared<FilterImage>(w, h);
    float m[20];
    auto matrix_type = this->type().value();
    if(matrix_type == ColorMatrixType::Matrix) { 
        const auto& v = this->values().values();
        for(int i = 0; i < 20; i++) m[i] = i < v.size() ? v[i] : 0.f;
    } else if(matrix_type == ColorMatrixType::Saturate) {
        float s = this->values().values().empty() ? 1.f : this->values().values()[0];
        float m_s[] = { 0.213f+0.787f*s, 0.715f-0.715f*s, 0.072f-0.072f*s, 0, 0, 0.213f-0.213f*s, 0.715f+0.285f*s, 0.072f-0.072f*s, 0, 0,
                        0.213f-0.213f*s, 0.715f-0.715f*s, 0.072f+0.928f*s, 0, 0, 0, 0, 0, 1, 0 };
        std::memcpy(m, m_s, 20 * sizeof(float));
    } else if(matrix_type == ColorMatrixType::HueRotate) {
        float theta = (this->values().values().empty() ? 0.f : this->values().values()[0]) * (M_PI / 180.f);        
        float cosTheta = std::cos(theta), sinTheta = std::sin(theta);
        float m_h[] = { 0.213f + cosTheta*0.787f - sinTheta*0.213f, 0.715f - cosTheta*0.715f - sinTheta*0.715f, 0.072f - cosTheta*0.072f + sinTheta*0.928f, 0, 0,
                        0.213f - cosTheta*0.213f + sinTheta*0.143f, 0.715f + cosTheta*0.285f + sinTheta*0.140f, 0.072f - cosTheta*0.072f - sinTheta*0.283f, 0, 0,
                        0.213f - cosTheta*0.213f - sinTheta*0.787f, 0.715f - cosTheta*0.715f + sinTheta*0.715f, 0.072f + cosTheta*0.928f + sinTheta*0.072f, 0, 0,
                        0, 0, 0, 1, 0 };
        std::memcpy(m, m_h, 20 * sizeof(float));
    } else if(matrix_type == ColorMatrixType::LuminanceToAlpha) {
        float m_l[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125f, 0.7154f, 0.0721f, 0, 0 };
        std::memcpy(m, m_l, 20 * sizeof(float));
    }
    
    const FilterPixel* src = input->data();
    FilterPixel* dst = result->data();
    for(int i = 0; i < w * h; i++) {
        const auto& s = src[i]; if(s.a <= 0) continue;
        float invA = 1.f / s.a;
        float r = s.r * invA, g = s.g * invA, b = s.b * invA, a = s.a;
        float nr = m[0]*r + m[1]*g + m[2]*b + m[3]*a + m[4], ng = m[5]*r + m[6]*g + m[7]*b + m[8]*a + m[9], nb = m[10]*r + m[11]*g + m[12]*b + m[13]*a + m[14], na = std::clamp(m[15]*r + m[16]*g + m[17]*b + m[18]*a + m[19], 0.f, 1.f);
        dst[i] = { nr * na, ng * na, nb * na, na };
    }
    context.addResult(this->result().value(), result);
}

void SVGFeBlendElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value()), input2 = context.getInput(this->m_in2.value()); if(!input || !input2) return;
    int w = input->width(), h = input->height(); auto result = std::make_shared<FilterImage>(w, h);
    FEBlendMode blend_mode = this->mode().value();
    const FilterPixel* src1 = input->data();
    const FilterPixel* src2 = input2->data();
    FilterPixel* dst = result->data();
    for(int i = 0; i < w * h; i++) {
        const auto& s = src1[i], &d = src2[i];
        if(blend_mode == FEBlendMode::Normal) { dst[i] = { s.r + d.r * (1-s.a), s.g + d.g * (1-s.a), s.b + d.b * (1-s.a), s.a + d.a * (1-s.a) }; }
        else {
            float sa = s.a, da = d.a;
            float invSa = sa > 0 ? 1.f / sa : 0, invDa = da > 0 ? 1.f / da : 0;
            float un_s_r = s.r * invSa, un_s_g = s.g * invSa, un_s_b = s.b * invSa;
            float un_d_r = d.r * invDa, un_d_g = d.g * invDa, un_d_b = d.b * invDa;
            
            auto blend_fn = [&](float un_s, float un_d, float s_c, float d_c) {
                float res = 0;
                if(blend_mode == FEBlendMode::Multiply) res = un_s * un_d;
                else if(blend_mode == FEBlendMode::Screen) res = un_s + un_d - un_s * un_d;
                else if(blend_mode == FEBlendMode::Darken) res = std::min(un_s, un_d);
                else if(blend_mode == FEBlendMode::Lighten) res = std::max(un_s, un_d);
                return (res * sa * da + s_c * (1 - da) + d_c * (1 - sa));
            };
            dst[i] = { blend_fn(un_s_r, un_d_r, s.r, d.r), blend_fn(un_s_g, un_d_g, s.g, d.g), blend_fn(un_s_b, un_d_b, s.b, d.b), sa + da - sa * da };
        }
    }
    context.addResult(this->result().value(), result);
}

// --- boilerplate ---
SVGFilterElement::SVGFilterElement(Document* d) : SVGElement(d, ElementID::Filter), m_x(PropertyID::X, LengthDirection::Horizontal, LengthNegativeMode::Allow, -10.f, LengthUnits::Percent), m_y(PropertyID::Y, LengthDirection::Vertical, LengthNegativeMode::Allow, -10.f, LengthUnits::Percent), m_width(PropertyID::Width, LengthDirection::Horizontal, LengthNegativeMode::Forbid, 120.f, LengthUnits::Percent), m_height(PropertyID::Height, LengthDirection::Vertical, LengthNegativeMode::Forbid, 120.f, LengthUnits::Percent), m_filterUnits(PropertyID::FilterUnits, Units::ObjectBoundingBox), m_primitiveUnits(PropertyID::PrimitiveUnits, Units::UserSpaceOnUse) { addProperty(m_x); addProperty(m_y); addProperty(m_width); addProperty(m_height); addProperty(m_filterUnits); addProperty(m_primitiveUnits); }
std::shared_ptr<Canvas> SVGFilterElement::applyFilter(const SVGRenderState& s, const Canvas& sg) const { FilterContext c(this, s.element(), s, sg); for(const auto& ch : children()) { if(auto p = toSVGElement(ch)) if(p->isFilterPrimitiveElement()) static_cast<const SVGFilterPrimitiveElement*>(p)->render(c); } return c.lastResult->toCanvas(sg.extents()); }
SVGFilterPrimitiveElement::SVGFilterPrimitiveElement(Document* d, ElementID id) : SVGElement(d, id), m_in(PropertyID::In), m_result(PropertyID::Result), m_x(PropertyID::X, LengthDirection::Horizontal, LengthNegativeMode::Allow, 0.f, LengthUnits::Percent), m_y(PropertyID::Y, LengthDirection::Vertical, LengthNegativeMode::Allow, 0.f, LengthUnits::Percent), m_width(PropertyID::Width, LengthDirection::Horizontal, LengthNegativeMode::Forbid, 100.f, LengthUnits::Percent), m_height(PropertyID::Height, LengthDirection::Vertical, LengthNegativeMode::Forbid, 100.f, LengthUnits::Percent) { addProperty(m_in); addProperty(m_result); addProperty(m_x); addProperty(m_y); addProperty(m_width); addProperty(m_height); }
SVGFeGaussianBlurElement::SVGFeGaussianBlurElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeGaussianBlur), m_stdDeviation(PropertyID::StdDeviation) { addProperty(m_stdDeviation); }
SVGFeOffsetElement::SVGFeOffsetElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeOffset), m_dx(PropertyID::Dx, 0.f), m_dy(PropertyID::Dy, 0.f) { addProperty(m_dx); addProperty(m_dy); }
SVGFeDropShadowElement::SVGFeDropShadowElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeDropShadow), m_stdDeviation(PropertyID::StdDeviation), m_dx(PropertyID::Dx, 2.f), m_dy(PropertyID::Dy, 2.f) { addProperty(m_stdDeviation); addProperty(m_dx); addProperty(m_dy); }
void SVGFeDropShadowElement::layoutElement(const SVGLayoutState& s) { m_flood_color = s.flood_color(); m_flood_opacity = s.flood_opacity(); SVGElement::layoutElement(s); }
SVGFeMergeNodeElement::SVGFeMergeNodeElement(Document* d) : SVGElement(d, ElementID::FeMergeNode), m_in(PropertyID::In) { addProperty(m_in); }
SVGFeMergeElement::SVGFeMergeElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeMerge) {}
SVGFeFloodElement::SVGFeFloodElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeFlood) {}
void SVGFeFloodElement::layoutElement(const SVGLayoutState& s) { m_flood_color = s.flood_color(); m_flood_opacity = s.flood_opacity(); SVGElement::layoutElement(s); }
void SVGFeFloodElement::render(FilterContext& c) const { int w = c.sourceGraphic->width(), h = c.sourceGraphic->height(); auto r = std::make_shared<FilterImage>(w, h); float ra = m_flood_opacity, rr = toLinear(m_flood_color.redF()) * ra, rg = toLinear(m_flood_color.greenF()) * ra, rb = toLinear(m_flood_color.blueF()) * ra; FilterPixel* dst = r->data(); for(int i=0; i<w*h; i++) dst[i] = {rr, rg, rb, ra}; c.addResult(this->result().value(), r); }
SVGFeBlendElement::SVGFeBlendElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeBlend), m_in2(PropertyID::In2), m_mode(PropertyID::Mode, FEBlendMode::Normal) { addProperty(m_in2); addProperty(m_mode); }
SVGFeCompositeElement::SVGFeCompositeElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeComposite), m_in2(PropertyID::In2), m_operator(PropertyID::Operator, FECompositeOperator::Over), m_k1(PropertyID::K1, 0.f), m_k2(PropertyID::K2, 0.f) , m_k3(PropertyID::K3, 0.f), m_k4(PropertyID::K4, 0.f) { addProperty(m_in2); addProperty(m_operator); addProperty(m_k1); addProperty(m_k2); addProperty(m_k3); addProperty(m_k4); }
SVGFeColorMatrixElement::SVGFeColorMatrixElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeColorMatrix), m_type(PropertyID::Type, ColorMatrixType::Matrix), m_values(PropertyID::Values) { addProperty(m_type); addProperty(m_values); }

} // namespace lunasvg