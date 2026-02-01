#include "filterelement.h"
#include "renderstate.h"
#include "layoutstate.h"

#include <cmath>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lunasvg {

// --- Color Space Conversion Helpers ---
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
    int w = canvas.width();
    int h = canvas.height();
    auto image = std::make_shared<FilterImage>(w, h);
    Bitmap bitmap(plutovg_surface_reference(canvas.surface()));
    const uint8_t* data = bitmap.data();
    int stride = bitmap.stride();
    FilterPixel* dst = image->data();
    for(int y = 0; y < h; ++y) {
        for(int x = 0; x < w; ++x) {
            int i = y * stride + x * 4;
            float a = data[i+3] / 255.f;
            if(a > 0) {
                dst[y*w + x].r = toLinear((data[i+2] / 255.f) / a) * a;
                dst[y*w + x].g = toLinear((data[i+1] / 255.f) / a) * a;
                dst[y*w + x].b = toLinear((data[i+0] / 255.f) / a) * a;
                dst[y*w + x].a = a;
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
        for(int x = 0; x < m_width; ++x) {
            int i = y * stride + x * 4;
            const auto& p = src[y * m_width + x];
            float a = std::clamp(p.a, 0.f, 1.f);
            if(a > 0.0001f) {
                data[i+3] = toByte(a);
                data[i+2] = toByte(toSRGB(std::clamp(p.r / a, 0.f, 1.f)) * a);
                data[i+1] = toByte(toSRGB(std::clamp(p.g / a, 0.f, 1.f)) * a);
                data[i+0] = toByte(toSRGB(std::clamp(p.b / a, 0.f, 1.f)) * a);
            } else {
                std::memset(&data[i], 0, 4);
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
    for(int i = 0; i < w * h; ++i) this->sourceAlpha->data()[i] = {0, 0, 0, this->sourceGraphic->data()[i].a};
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
static void boxBlur(FilterPixel* src, FilterPixel* dst, int w, int h, int r, bool horizontal) {
    if (r <= 0) return;
    float iarr = 1.f / (r + r + 1);
    for(int i = 0; i < (horizontal ? h : w); i++) {
        int head = horizontal ? i * w : i;
        int ti = head;
        FilterPixel fv = src[head], val = {(r + 1) * fv.r, (r + 1) * fv.g, (r + 1) * fv.b, (r + 1) * fv.a};
        for(int j = 0; j < r; j++) {
            const auto& p = src[head + std::min(j, (horizontal ? w : h) - 1) * (horizontal ? 1 : w)];
            val.r += p.r; val.g += p.g; val.b += p.b; val.a += p.a;
        }
        for(int j = 0; j < (horizontal ? w : h); j++) {
            int cur_ri = head + std::min(j + r, (horizontal ? w : h) - 1) * (horizontal ? 1 : w);
            int cur_li = head + std::max(j - r - 1, 0) * (horizontal ? 1 : w);
            const auto& pri = src[cur_ri];
            const auto& pli = src[cur_li];
            if (j > 0) {
                val.r += pri.r - pli.r; val.g += pri.g - pli.g; val.b += pri.b - pli.b; val.a += pri.a - pli.a;
            }
            dst[ti] = {val.r * iarr, val.g * iarr, val.b * iarr, val.a * iarr};
            ti += (horizontal ? 1 : w);
        }
    }
}

void SVGFeGaussianBlurElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value());
    if(!input) return;
    int w = input->width(), h = input->height();
    auto result = std::make_shared<FilterImage>(w, h);
    auto temp = std::make_shared<FilterImage>(w, h);
    std::memcpy(result->data(), input->data(), w * h * sizeof(FilterPixel));
    float stdDevX = 0, stdDevY = 0;
    if(!m_stdDeviation.values().empty()) {
        stdDevX = m_stdDeviation.values()[0];
        stdDevY = m_stdDeviation.values().size() > 1 ? m_stdDeviation.values()[1] : stdDevX;
    }
    if(stdDevX > 0 || stdDevY > 0) {
        int rx = (int)std::floor(stdDevX * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        int ry = (int)std::floor(stdDevY * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        for(int i = 0; i < 3; i++) {
            if(rx > 0) { boxBlur(result->data(), temp->data(), w, h, rx, true); std::memcpy(result->data(), temp->data(), w * h * sizeof(FilterPixel)); }
            if(ry > 0) { boxBlur(result->data(), temp->data(), w, h, ry, false); std::memcpy(result->data(), temp->data(), w * h * sizeof(FilterPixel)); }
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeOffsetElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value());
    if(!input) return;
    int w = input->width(), h = input->height();
    auto result = std::make_shared<FilterImage>(w, h);
    int ox = (int)std::round(this->dx().value());
    int oy = (int)std::round(this->dy().value());
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            int sx = x - ox, sy = y - oy;
            if(sx >= 0 && sx < w && sy >= 0 && sy < h) result->data()[y * w + x] = input->data()[sy * w + sx];
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeDropShadowElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value());
    if(!input) return;
    int w = input->width(), h = input->height();
    
    // 1. Create Shadow Alpha
    auto shadow = std::make_shared<FilterImage>(w, h);
    float ra = m_flood_opacity, rr = toLinear(m_flood_color.redF()) * ra, rg = toLinear(m_flood_color.greenF()) * ra, rb = toLinear(m_flood_color.blueF()) * ra;
    for(int i = 0; i < w * h; i++) shadow->data()[i] = {rr, rg, rb, input->data()[i].a * ra};

    // 2. Blur shadow
    float stdDevX = 0, stdDevY = 0;
    if(!m_stdDeviation.values().empty()) {
        stdDevX = m_stdDeviation.values()[0];
        stdDevY = m_stdDeviation.values().size() > 1 ? m_stdDeviation.values()[1] : stdDevX;
    }
    if(stdDevX > 0 || stdDevY > 0) {
        auto temp = std::make_shared<FilterImage>(w, h);
        int rx = (int)std::floor(stdDevX * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        int ry = (int)std::floor(stdDevY * 3.f * std::sqrt(2.f * M_PI) / 4.f + 0.5f) / 2;
        for(int i = 0; i < 3; i++) {
            if(rx > 0) { boxBlur(shadow->data(), temp->data(), w, h, rx, true); std::memcpy(shadow->data(), temp->data(), w * h * sizeof(FilterPixel)); }
            if(ry > 0) { boxBlur(shadow->data(), temp->data(), w, h, ry, false); std::memcpy(shadow->data(), temp->data(), w * h * sizeof(FilterPixel)); }
        }
    }

    // 3. Offset and Merge
    auto result = std::make_shared<FilterImage>(w, h);
    int ox = (int)std::round(this->dx().value()), oy = (int)std::round(this->dy().value());
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            int sx = x - ox, sy = y - oy;
            FilterPixel s = (sx >= 0 && sx < w && sy >= 0 && sy < h) ? shadow->data()[sy * w + sx] : FilterPixel{0,0,0,0};
            const auto& g = input->data()[y * w + x];
            result->data()[y * w + x] = { g.r + s.r * (1.f - g.a), g.g + s.g * (1.f - g.a), g.b + s.b * (1.f - g.a), g.a + s.a * (1.f - g.a) };
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
                for(int i = 0; i < w * h; i++) {
                    const auto& s = input->data()[i]; auto& d = result->data()[i];
                    d.r = s.r + d.r * (1.f - s.a); d.g = s.g + d.g * (1.f - s.a); d.b = s.b + d.b * (1.f - s.a); d.a = s.a + d.a * (1.f - s.a);
                }
            }
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeCompositeElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value());
    auto input2 = context.getInput(this->m_in2.value());
    if(!input || !input2) return;
    int w = input->width(), h = input->height();
    auto result = std::make_shared<FilterImage>(w, h);
    float fk1 = m_k1.value(), fk2 = m_k2.value(), fk3 = m_k3.value(), fk4 = m_k4.value();
    for(int i = 0; i < w * h; i++) {
        const auto& s1 = input->data()[i], &s2 = input2->data()[i];
        if(this->_operator().value() == FECompositeOperator::Arithmetic) {
            float na = std::clamp(fk1 * s1.a * s2.a + fk2 * s1.a + fk3 * s2.a + fk4, 0.f, 1.f);
            if(na > 0) {
                float c1r = s1.a > 0 ? s1.r / s1.a : 0, c1g = s1.a > 0 ? s1.g / s1.a : 0, c1b = s1.a > 0 ? s1.b / s1.a : 0;
                float c2r = s2.a > 0 ? s2.r / s2.a : 0, c2g = s2.a > 0 ? s2.g / s2.a : 0, c2b = s2.a > 0 ? s2.b / s2.a : 0;
                result->data()[i] = { std::clamp(fk1*c1r*c2r + fk2*c1r + fk3*c2r + fk4, 0.f, 1.f) * na,
                                       std::clamp(fk1*c1g*c2g + fk2*c1g + fk3*c2g + fk4, 0.f, 1.f) * na,
                                       std::clamp(fk1*c1b*c2b + fk2*c1b + fk3*c2b + fk4, 0.f, 1.f) * na, na };
            }
        } else {
            float fa = 0, fb = 0;
            switch(this->_operator().value()) {
                case FECompositeOperator::Over: fa = 1; fb = 1 - s1.a; break;
                case FECompositeOperator::In: fa = s2.a; fb = 0; break;
                case FECompositeOperator::Out: fa = 1 - s2.a; fb = 0; break;
                case FECompositeOperator::Atop: fa = s2.a; fb = 1 - s1.a; break;
                case FECompositeOperator::Xor: fa = 1 - s2.a; fb = 1 - s1.a; break;
                default: break;
            }
            result->data()[i] = { s1.r * fa + s2.r * fb, s1.g * fa + s2.g * fb, s1.b * fa + s2.b * fb, s1.a * fa + s2.a * fb };
        }
    }
    context.addResult(this->result().value(), result);
}

void SVGFeColorMatrixElement::render(FilterContext& context) const {
    auto input = context.getInput(this->in().value());
    if(!input) return;
    int w = input->width(), h = input->height();
    auto result = std::make_shared<FilterImage>(w, h);
    std::vector<float> m;
    auto matrix_type = this->type().value();
    if(matrix_type == ColorMatrixType::Matrix) {
        m = this->values().values(); if(m.size() < 20) m.assign(20, 0.f);
    } else if(matrix_type == ColorMatrixType::Saturate) {
        float s = this->values().values().empty() ? 1.f : this->values().values()[0];
        m = { 0.213f+0.787f*s, 0.715f-0.715f*s, 0.072f-0.072f*s, 0, 0, 0.213f-0.213f*s, 0.715f+0.285f*s, 0.072f-0.072f*s, 0, 0, 
              0.213f-0.213f*s, 0.715f-0.715f*s, 0.072f+0.928f*s, 0, 0, 0, 0, 0, 1, 0 };
    } else if(matrix_type == ColorMatrixType::HueRotate) {
        float theta = (this->values().values().empty() ? 0.f : this->values().values()[0]) * (M_PI / 180.f);
        float cosTheta = std::cos(theta), sinTheta = std::sin(theta);
        m = { 0.213f + cosTheta*0.787f - sinTheta*0.213f, 0.715f - cosTheta*0.715f - sinTheta*0.715f, 0.072f - cosTheta*0.072f + sinTheta*0.928f, 0, 0,
              0.213f - cosTheta*0.213f + sinTheta*0.143f, 0.715f + cosTheta*0.285f + sinTheta*0.140f, 0.072f - cosTheta*0.072f - sinTheta*0.283f, 0, 0,
              0.213f - cosTheta*0.213f - sinTheta*0.787f, 0.715f - cosTheta*0.715f + sinTheta*0.715f, 0.072f + cosTheta*0.928f + sinTheta*0.072f, 0, 0,
              0, 0, 0, 1, 0 };
    } else if(matrix_type == ColorMatrixType::LuminanceToAlpha) {
        m = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125f, 0.7154f, 0.0721f, 0, 0 };
    }
    if (m.empty()) { std::memcpy(result->data(), input->data(), w * h * sizeof(FilterPixel)); }
    else {
        for(int i = 0; i < w * h; i++) {
            const auto& s = input->data()[i]; if(s.a <= 0) continue;
            float r = s.r / s.a, g = s.g / s.a, b = s.b / s.a;
            float nr = m[0]*r + m[1]*g + m[2]*b + m[3]*s.a + m[4], ng = m[5]*r + m[6]*g + m[7]*b + m[8]*s.a + m[9], nb = m[10]*r + m[11]*g + m[12]*b + m[13]*s.a + m[14], na = std::clamp(m[15]*r + m[16]*g + m[17]*b + m[18]*s.a + m[19], 0.f, 1.f);
            result->data()[i] = { nr * na, ng * na, nb * na, na };
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
void SVGFeFloodElement::render(FilterContext& c) const { int w = c.sourceGraphic->width(), h = c.sourceGraphic->height(); auto r = std::make_shared<FilterImage>(w, h); float ra = m_flood_opacity, rr = toLinear(m_flood_color.redF()) * ra, rg = toLinear(m_flood_color.greenF()) * ra, rb = toLinear(m_flood_color.blueF()) * ra; for(int i=0; i<w*h; i++) r->data()[i] = {rr, rg, rb, ra}; c.addResult(this->result().value(), r); }
SVGFeBlendElement::SVGFeBlendElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeBlend), m_in2(PropertyID::In2), m_mode(PropertyID::Mode, FEBlendMode::Normal) { addProperty(m_in2); addProperty(m_mode); }
void SVGFeBlendElement::render(FilterContext& c) const { auto i1 = c.getInput(this->in().value()), i2 = c.getInput(this->m_in2.value()); if(!i1 || !i2) return; int w = i1->width(), h = i1->height(); auto r = std::make_shared<FilterImage>(w, h); for(int i=0; i<w*h; i++) { const auto& s = i1->data()[i], &d = i2->data()[i]; r->data()[i] = { s.r + d.r * (1-s.a), s.g + d.g * (1-s.a), s.b + d.b * (1-s.a), s.a + d.a * (1-s.a) }; } c.addResult(this->result().value(), r); }
SVGFeCompositeElement::SVGFeCompositeElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeComposite), m_in2(PropertyID::In2), m_operator(PropertyID::Operator, FECompositeOperator::Over), m_k1(PropertyID::K1, 0.f), m_k2(PropertyID::K2, 0.f) , m_k3(PropertyID::K3, 0.f), m_k4(PropertyID::K4, 0.f) { addProperty(m_in2); addProperty(m_operator); addProperty(m_k1); addProperty(m_k2); addProperty(m_k3); addProperty(m_k4); }
SVGFeColorMatrixElement::SVGFeColorMatrixElement(Document* d) : SVGFilterPrimitiveElement(d, ElementID::FeColorMatrix), m_type(PropertyID::Type, ColorMatrixType::Matrix), m_values(PropertyID::Values) { addProperty(m_type); addProperty(m_values); }

} // namespace lunasvg
