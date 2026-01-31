#include "filterelement.h"
#include "renderstate.h"
#include "layoutstate.h"

#include <cmath>
#include <vector>
#include <cstring>

namespace lunasvg {

FilterContext::FilterContext(const SVGFilterElement* filter, const SVGElement* element, const SVGRenderState& state, const Canvas& sourceGraphic)
    : filter(filter)
    , element(element)
    , state(state)
{
    auto canvas = Canvas::create(sourceGraphic.extents());
    canvas->blendCanvas(sourceGraphic, BlendMode::Src, 1.0);
    this->sourceGraphic = canvas;

    // Create SourceAlpha
    Bitmap srcBitmap(plutovg_surface_reference(sourceGraphic.surface()));
    Bitmap alphaBitmap(srcBitmap.width(), srcBitmap.height());
    uint8_t* srcData = srcBitmap.data();
    uint8_t* alphaData = alphaBitmap.data();
    int stride = srcBitmap.stride();
    int height = srcBitmap.height();
    int width = srcBitmap.width();

    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            int i = y * stride + x * 4;
            // Assumes ARGB32 (BGRA in memory on little-endian)
            alphaData[i] = 0;
            alphaData[i+1] = 0;
            alphaData[i+2] = 0;
            alphaData[i+3] = srcData[i+3];
        }
    }
    this->sourceAlpha = Canvas::create(sourceGraphic.x(), sourceGraphic.y(), (float)width, (float)height);
    this->sourceAlpha->drawImage(alphaBitmap, sourceGraphic.extents(), Rect(0, 0, (float)width, (float)height), Transform::Identity);

    results.emplace("SourceGraphic", this->sourceGraphic);
    results.emplace("SourceAlpha", this->sourceAlpha);
    lastResult = this->sourceGraphic;
}

std::shared_ptr<Canvas> FilterContext::getInput(const std::string& in)
{
    if(in.empty())
        return lastResult;

    auto it = results.find(in);
    if(it != results.end())
        return it->second;

    return nullptr;
}

void FilterContext::addResult(const std::string& result, std::shared_ptr<Canvas> image)
{
    lastResult = image;
    if(!result.empty())
        results[result] = std::move(image);
}

SVGFilterElement::SVGFilterElement(Document* document)
    : SVGElement(document, ElementID::Filter)
    , m_x(PropertyID::X, LengthDirection::Horizontal, LengthNegativeMode::Allow, -10.f, LengthUnits::Percent)
    , m_y(PropertyID::Y, LengthDirection::Vertical, LengthNegativeMode::Allow, -10.f, LengthUnits::Percent)
    , m_width(PropertyID::Width, LengthDirection::Horizontal, LengthNegativeMode::Forbid, 120.f, LengthUnits::Percent)
    , m_height(PropertyID::Height, LengthDirection::Vertical, LengthNegativeMode::Forbid, 120.f, LengthUnits::Percent)
    , m_filterUnits(PropertyID::FilterUnits, Units::ObjectBoundingBox)
    , m_primitiveUnits(PropertyID::PrimitiveUnits, Units::UserSpaceOnUse)
{
    addProperty(m_x);
    addProperty(m_y);
    addProperty(m_width);
    addProperty(m_height);
    addProperty(m_filterUnits);
    addProperty(m_primitiveUnits);
}

std::shared_ptr<Canvas> SVGFilterElement::applyFilter(const SVGRenderState& state, const Canvas& sourceGraphic) const
{
    FilterContext context(this, state.element(), state, sourceGraphic);

    for(const auto& child : children()) {
        if(auto primitive = toSVGElement(child)) {
             if(primitive->isFilterPrimitiveElement()) {
                 static_cast<const SVGFilterPrimitiveElement*>(primitive)->render(context);
             }
        }
    }

    return context.lastResult;
}

SVGFilterPrimitiveElement::SVGFilterPrimitiveElement(Document* document, ElementID id)
    : SVGElement(document, id)
    , m_in(PropertyID::In)
    , m_result(PropertyID::Result)
    , m_x(PropertyID::X, LengthDirection::Horizontal, LengthNegativeMode::Allow, 0.f, LengthUnits::Percent)
    , m_y(PropertyID::Y, LengthDirection::Vertical, LengthNegativeMode::Allow, 0.f, LengthUnits::Percent)
    , m_width(PropertyID::Width, LengthDirection::Horizontal, LengthNegativeMode::Forbid, 100.f, LengthUnits::Percent)
    , m_height(PropertyID::Height, LengthDirection::Vertical, LengthNegativeMode::Forbid, 100.f, LengthUnits::Percent)
{
    addProperty(m_in);
    addProperty(m_result);
    addProperty(m_x);
    addProperty(m_y);
    addProperty(m_width);
    addProperty(m_height);
}

SVGFeGaussianBlurElement::SVGFeGaussianBlurElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeGaussianBlur)
    , m_stdDeviation(PropertyID::StdDeviation)
{
    addProperty(m_stdDeviation);
}

static void boxBlurH(uint8_t* scl, uint8_t* tcl, int w, int h, int stride, int r)
{
    float iarr = 1.f / (r+r+1);
    for(int i=0; i<h; i++) {
        int ti = i*stride;
        int li = ti;
        int ri = ti+r*4;

        int fv[4] = {scl[ti], scl[ti+1], scl[ti+2], scl[ti+3]};
        int lv[4] = {scl[ti+w*4-4], scl[ti+w*4-3], scl[ti+w*4-2], scl[ti+w*4-1]};

        int val[4] = {
            (r+1)*fv[0],
            (r+1)*fv[1],
            (r+1)*fv[2],
            (r+1)*fv[3]
        };

        for(int j=0; j<r; j++) {
            val[0] += scl[ti+j*4];
            val[1] += scl[ti+j*4+1];
            val[2] += scl[ti+j*4+2];
            val[3] += scl[ti+j*4+3];
        }

        for(int j=0; j<=r; j++) {
            val[0] += scl[ri] - fv[0];
            val[1] += scl[ri+1] - fv[1];
            val[2] += scl[ri+2] - fv[2];
            val[3] += scl[ri+3] - fv[3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            ri+=4; ti+=4;
        }

        for(int j=r+1; j<w-r; j++) {
            val[0] += scl[ri] - scl[li];
            val[1] += scl[ri+1] - scl[li+1];
            val[2] += scl[ri+2] - scl[li+2];
            val[3] += scl[ri+3] - scl[li+3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            li+=4; ri+=4; ti+=4;
        }

        for(int j=w-r; j<w; j++) {
            val[0] += lv[0] - scl[li];
            val[1] += lv[1] - scl[li+1];
            val[2] += lv[2] - scl[li+2];
            val[3] += lv[3] - scl[li+3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            li+=4; ti+=4;
        }
    }
}

static void boxBlurV(uint8_t* scl, uint8_t* tcl, int w, int h, int stride, int r)
{
    float iarr = 1.f / (r+r+1);
    for(int i=0; i<w; i++) {
        int ti = i*4;
        int li = ti;
        int ri = ti+r*stride;

        int fv[4] = {scl[ti], scl[ti+1], scl[ti+2], scl[ti+3]};
        int lv[4] = {scl[ti+(h-1)*stride], scl[ti+(h-1)*stride+1], scl[ti+(h-1)*stride+2], scl[ti+(h-1)*stride+3]};

        int val[4] = {
            (r+1)*fv[0],
            (r+1)*fv[1],
            (r+1)*fv[2],
            (r+1)*fv[3]
        };

        for(int j=0; j<r; j++) {
            val[0] += scl[ti+j*stride];
            val[1] += scl[ti+j*stride+1];
            val[2] += scl[ti+j*stride+2];
            val[3] += scl[ti+j*stride+3];
        }

        for(int j=0; j<=r; j++) {
            val[0] += scl[ri] - fv[0];
            val[1] += scl[ri+1] - fv[1];
            val[2] += scl[ri+2] - fv[2];
            val[3] += scl[ri+3] - fv[3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            ri+=stride; ti+=stride;
        }

        for(int j=r+1; j<h-r; j++) {
            val[0] += scl[ri] - scl[li];
            val[1] += scl[ri+1] - scl[li+1];
            val[2] += scl[ri+2] - scl[li+2];
            val[3] += scl[ri+3] - scl[li+3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            li+=stride; ri+=stride; ti+=stride;
        }

        for(int j=h-r; j<h; j++) {
            val[0] += lv[0] - scl[li];
            val[1] += lv[1] - scl[li+1];
            val[2] += lv[2] - scl[li+2];
            val[3] += lv[3] - scl[li+3];

            tcl[ti] = (uint8_t)std::round(val[0]*iarr);
            tcl[ti+1] = (uint8_t)std::round(val[1]*iarr);
            tcl[ti+2] = (uint8_t)std::round(val[2]*iarr);
            tcl[ti+3] = (uint8_t)std::round(val[3]*iarr);

            li+=stride; ti+=stride;
        }
    }
}

static void boxBlur(Bitmap& bitmap, int r)
{
    if(r <= 0) return;
    int w = bitmap.width();
    int h = bitmap.height();
    int stride = bitmap.stride();
    uint8_t* data = bitmap.data();

    std::vector<uint8_t> temp(stride * h);

    boxBlurH(data, temp.data(), w, h, stride, r);
    boxBlurV(temp.data(), data, w, h, stride, r);
}

void SVGFeGaussianBlurElement::render(FilterContext& context) const
{
    auto input = context.getInput(in().value());
    if(!input) return;

    Bitmap srcBitmap(plutovg_surface_reference(input->surface()));
    Bitmap dstBitmap(srcBitmap.width(), srcBitmap.height());
    std::memcpy(dstBitmap.data(), srcBitmap.data(), srcBitmap.stride() * srcBitmap.height());        

    float stdDevX = 0;
    float stdDevY = 0;
    if(m_stdDeviation.values().size() >= 1) stdDevX = m_stdDeviation.values()[0];
    if(m_stdDeviation.values().size() >= 2) stdDevY = m_stdDeviation.values()[1];
    else if(!m_stdDeviation.values().empty()) stdDevX = stdDevY = m_stdDeviation.values()[0];        

    if(stdDevX > 0 || stdDevY > 0) {
        int r = static_cast<int>(stdDevX * 1.5f);
        boxBlur(dstBitmap, r);
        boxBlur(dstBitmap, r);
        boxBlur(dstBitmap, r);
    }

    auto resultCanvas = Canvas::create(input->extents());
    resultCanvas->drawImage(dstBitmap, input->extents(), Rect(0, 0, (float)dstBitmap.width(), (float)dstBitmap.height()), Transform::Identity);
    context.addResult(result().value(), resultCanvas);
}

SVGFeOffsetElement::SVGFeOffsetElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeOffset)
    , m_dx(PropertyID::Dx, 0.f)
    , m_dy(PropertyID::Dy, 0.f)
{
    addProperty(m_dx);
    addProperty(m_dy);
}

void SVGFeOffsetElement::render(FilterContext& context) const
{
    auto input = context.getInput(in().value());
    if(!input) return;

    auto resultCanvas = Canvas::create(input->extents());
    Bitmap inputBitmap(plutovg_surface_reference(input->surface()));
    Rect dstRect = input->extents();
    dstRect.move(dx().value(), dy().value());
    resultCanvas->drawImage(inputBitmap, dstRect, Rect(0, 0, (float)inputBitmap.width(), (float)inputBitmap.height()), Transform::Identity);
    context.addResult(result().value(), resultCanvas);
}

SVGFeMergeNodeElement::SVGFeMergeNodeElement(Document* document)
    : SVGElement(document, ElementID::FeMergeNode)
    , m_in(PropertyID::In)
{
    addProperty(m_in);
}

SVGFeMergeElement::SVGFeMergeElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeMerge)
{
}

void SVGFeMergeElement::render(FilterContext& context) const
{
    auto resultCanvas = Canvas::create(context.sourceGraphic->extents());
    for(const auto& child : children()) {
        if(auto node = toSVGElement(child); node && node->id() == ElementID::FeMergeNode) {
            auto inputName = static_cast<const SVGFeMergeNodeElement*>(node)->in().value();
            auto input = context.getInput(inputName);
            if(input) {
                resultCanvas->blendCanvas(*input, BlendMode::Src_Over, 1.0);
            }
        }
    }
    context.addResult(result().value(), resultCanvas);
}

SVGFeFloodElement::SVGFeFloodElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeFlood)
{
}

void SVGFeFloodElement::layoutElement(const SVGLayoutState& state)
{
    m_flood_color = state.flood_color();
    m_flood_opacity = state.flood_opacity();
    SVGElement::layoutElement(state);
}

void SVGFeFloodElement::render(FilterContext& context) const
{
    auto resultCanvas = Canvas::create(context.sourceGraphic->extents());
    resultCanvas->setColor(m_flood_color.colorWithAlpha(m_flood_opacity));

    Path rectPath;
    rectPath.addRect(resultCanvas->extents());
    resultCanvas->fillPath(rectPath, FillRule::NonZero, Transform::Identity);

    context.addResult(result().value(), resultCanvas);
}

SVGFeBlendElement::SVGFeBlendElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeBlend)
    , m_in2(PropertyID::In2)
    , m_mode(PropertyID::Mode, FEBlendMode::Normal)
{
    addProperty(m_in2);
    addProperty(m_mode);
}

void SVGFeBlendElement::render(FilterContext& context) const
{
    auto input = context.getInput(in().value());
    auto input2 = context.getInput(m_in2.value());
    if(!input || !input2) return;

    auto resultCanvas = Canvas::create(input->extents());

    if(m_mode.value() == FEBlendMode::Normal) {
        resultCanvas->blendCanvas(*input2, BlendMode::Src, 1.0);
        resultCanvas->blendCanvas(*input, BlendMode::Src_Over, 1.0);
    } else {
        Bitmap src1(plutovg_surface_reference(input->surface()));
        Bitmap src2(plutovg_surface_reference(input2->surface()));
        Bitmap dst(src1.width(), src1.height());

        uint8_t* d = dst.data();
        uint8_t* s1 = src1.data();
        uint8_t* s2 = src2.data();
        int stride = dst.stride();
        int h = dst.height();
        int w = dst.width();

        FEBlendMode mode = m_mode.value();

        for(int y=0; y<h; ++y) {
            for(int x=0; x<w; ++x) {
                int i = y*stride + x*4;
                float a1 = s1[i+3]/255.f;
                float a2 = s2[i+3]/255.f;

                for(int c=0; c<3; ++c) {
                    float c1 = (s1[i+c]/255.f); // premultiplied
                    float c2 = (s2[i+c]/255.f); // premultiplied
                    float r = 0;

                    if(mode == FEBlendMode::Multiply) r = c1*c2 + c1*(1-a2) + c2*(1-a1);
                    else if(mode == FEBlendMode::Screen) r = c1 + c2 - c1*c2;
                    else if(mode == FEBlendMode::Darken) r = std::min(c1*a2, c2*a1) + c1*(1-a2) + c2*(1-a1);
                    else if(mode == FEBlendMode::Lighten) r = std::max(c1*a2, c2*a1) + c1*(1-a2) + c2*(1-a1);

                    d[i+c] = (uint8_t)(std::clamp(r, 0.f, 1.f) * 255);
                }
                d[i+3] = (uint8_t)(std::clamp(a1 + a2 - a1*a2, 0.f, 1.f) * 255);
            }
        }
        resultCanvas->drawImage(dst, input->extents(), Rect(0, 0, (float)w, (float)h), Transform::Identity);
    }

    context.addResult(result().value(), resultCanvas);
}

SVGFeCompositeElement::SVGFeCompositeElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeComposite)
    , m_in2(PropertyID::In2)
    , m_operator(PropertyID::Operator, FECompositeOperator::Over)
    , m_k1(PropertyID::K1, 0.f)
    , m_k2(PropertyID::K2, 0.f)
    , m_k3(PropertyID::K3, 0.f)
    , m_k4(PropertyID::K4, 0.f)
{
    addProperty(m_in2);
    addProperty(m_operator);
    addProperty(m_k1);
    addProperty(m_k2);
    addProperty(m_k3);
    addProperty(m_k4);
}

void SVGFeCompositeElement::render(FilterContext& context) const
{
    auto input = context.getInput(in().value());
    auto input2 = context.getInput(m_in2.value());
    if(!input || !input2) return;

    auto resultCanvas = Canvas::create(input->extents());

    if(m_operator.value() == FECompositeOperator::Arithmetic) {
        Bitmap src1(plutovg_surface_reference(input->surface()));
        Bitmap src2(plutovg_surface_reference(input2->surface()));
        Bitmap dst(src1.width(), src1.height());

        uint8_t* d = dst.data();
        uint8_t* s1 = src1.data();
        uint8_t* s2 = src2.data();
        int stride = dst.stride();
        int h = dst.height();
        int w = dst.width();

        float k1 = m_k1.value();
        float k2 = m_k2.value();
        float k3 = m_k3.value();
        float k4 = m_k4.value();

        for(int y=0; y<h; ++y) {
            for(int x=0; x<w; ++x) {
                int i = y*stride + x*4;
                float a1 = s1[i+3] / 255.f;
                float a2 = s2[i+3] / 255.f;
                
                // Result alpha
                float na = k1*a1*a2 + k2*a1 + k3*a2 + k4;
                na = std::clamp(na, 0.f, 1.f);
                
                for(int c=0; c<3; ++c) {
                    float i1 = s1[i+c] / 255.f;
                    float i2 = s2[i+c] / 255.f;
                    // Formula applied to premultiplied values results in premultiplied value
                    float res = k1*i1*i2 + k2*i1 + k3*i2 + k4;
                    // Clamping: 0 <= result <= alpha
                    d[i+c] = (uint8_t)(std::clamp(res, 0.f, na) * 255);
                }
                d[i+3] = (uint8_t)(na * 255);
            }
        }
        resultCanvas->drawImage(dst, input->extents(), Rect(0, 0, (float)w, (float)h), Transform::Identity);
    } else {
        BlendMode mode = BlendMode::Src_Over;
        switch(m_operator.value()) {
            case FECompositeOperator::Over: mode = BlendMode::Src_Over; break;
            case FECompositeOperator::In: mode = BlendMode::Src_In; break;
            case FECompositeOperator::Out: mode = BlendMode::Src_Out; break;
            case FECompositeOperator::Atop: mode = BlendMode::Src_Atop; break;
            case FECompositeOperator::Xor: mode = BlendMode::Xor; break;
            default: break;
        }
        resultCanvas->blendCanvas(*input2, BlendMode::Src, 1.0);
        resultCanvas->blendCanvas(*input, mode, 1.0);
    }

    context.addResult(result().value(), resultCanvas);
}

template<>
bool SVGEnumeration<ColorMatrixType>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<ColorMatrixType> entries[] = {
        {ColorMatrixType::Matrix, "matrix"},
        {ColorMatrixType::Saturate, "saturate"},
        {ColorMatrixType::HueRotate, "hueRotate"},
        {ColorMatrixType::LuminanceToAlpha, "luminanceToAlpha"}
    };

    return parseEnum(input, entries);
}

SVGFeColorMatrixElement::SVGFeColorMatrixElement(Document* document)
    : SVGFilterPrimitiveElement(document, ElementID::FeColorMatrix)
    , m_type(PropertyID::Type, ColorMatrixType::Matrix)
    , m_values(PropertyID::Values)
{
    addProperty(m_type);
    addProperty(m_values);
}

void SVGFeColorMatrixElement::render(FilterContext& context) const
{
    auto input = context.getInput(in().value());
    if(!input) return;

    Bitmap srcBitmap(plutovg_surface_reference(input->surface()));
    Bitmap dstBitmap(srcBitmap.width(), srcBitmap.height());

    uint8_t* srcData = srcBitmap.data();
    uint8_t* dstData = dstBitmap.data();
    int stride = srcBitmap.stride();
    int height = srcBitmap.height();
    int width = srcBitmap.width();

    std::vector<float> v;
    if(m_type.value() == ColorMatrixType::Matrix) {
        v = m_values.values();
        if(v.size() < 20) v.assign(20, 0.f);
    } else if(m_type.value() == ColorMatrixType::Saturate) {
        float s = m_values.values().empty() ? 1.f : m_values.values()[0];
        v = {
            0.213f+0.787f*s, 0.715f-0.715f*s, 0.072f-0.072f*s, 0, 0,
            0.213f-0.213f*s, 0.715f+0.285f*s, 0.072f-0.072f*s, 0, 0,
            0.213f-0.213f*s, 0.715f-0.715f*s, 0.072f+0.928f*s, 0, 0,
            0, 0, 0, 1, 0
        };
    } else if(m_type.value() == ColorMatrixType::HueRotate) {
        float theta = (m_values.values().empty() ? 0.f : m_values.values()[0]) * (PLUTOVG_PI / 180.f);
        float c = std::cos(theta);
        float s = std::sin(theta);
        v = {
            0.213f + c*0.787f - s*0.213f, 0.715f - c*0.715f - s*0.715f, 0.072f - c*0.072f + s*0.928f, 0, 0,
            0.213f - c*0.213f + s*0.143f, 0.715f + c*0.285f + s*0.140f, 0.072f - c*0.072f - s*0.283f, 0, 0,
            0.213f - c*0.213f - s*0.787f, 0.715f - c*0.715f + s*0.715f, 0.072f + c*0.928f + s*0.072f, 0, 0,
            0, 0, 0, 1, 0
        };
    } else if(m_type.value() == ColorMatrixType::LuminanceToAlpha) {
        v = {
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            0.2125f, 0.7154f, 0.0721f, 0, 0
        };
    }

    if(!v.empty()) {
        for(int y = 0; y < height; ++y) {
            for(int x = 0; x < width; ++x) {
                int i = y * stride + x * 4;
                float a = srcData[i+3] / 255.f;
                if(a <= 0) {
                    std::memset(&dstData[i], 0, 4);
                    continue;
                }
                float b = (srcData[i+0] / 255.f) / a;
                float g = (srcData[i+1] / 255.f) / a;
                float r = (srcData[i+2] / 255.f) / a;

                float nr = v[0]*r + v[1]*g + v[2]*b + v[3]*a + v[4];
                float ng = v[5]*r + v[6]*g + v[7]*b + v[8]*a + v[9];
                float nb = v[10]*r + v[11]*g + v[12]*b + v[13]*a + v[14];
                float na = v[15]*r + v[16]*g + v[17]*b + v[18]*a + v[19];

                na = std::clamp(na, 0.f, 1.f);
                dstData[i+3] = (uint8_t)(na * 255);
                dstData[i+2] = (uint8_t)(std::clamp(nr * na, 0.f, na) * 255);
                dstData[i+1] = (uint8_t)(std::clamp(ng * na, 0.f, na) * 255);
                dstData[i+0] = (uint8_t)(std::clamp(nb * na, 0.f, na) * 255);
            }
        }
    } else {
        std::memcpy(dstData, srcData, stride * height);
    }

    auto resultCanvas = Canvas::create(input->extents());
    resultCanvas->drawImage(dstBitmap, input->extents(), Rect(0, 0, (float)dstBitmap.width(), (float)dstBitmap.height()), Transform::Identity);
    context.addResult(result().value(), resultCanvas);
}

} // namespace lunasvg
