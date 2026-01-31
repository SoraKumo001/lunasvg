#include "property.h"
#include "element.h"
#include "parserutils.h"

#include <cassert>

namespace lunasvg {

PropertyID propertyid(const std::string_view& name)
{
    static const struct {
        std::string_view name;
        PropertyID value;
    } table[] = {
        {"class", PropertyID::Class},
        {"clipPathUnits", PropertyID::ClipPathUnits},
        {"cx", PropertyID::Cx},
        {"cy", PropertyID::Cy},
        {"d", PropertyID::D},
        {"dx", PropertyID::Dx},
        {"dy", PropertyID::Dy},
        {"filterUnits", PropertyID::FilterUnits},
        {"fx", PropertyID::Fx},
        {"fy", PropertyID::Fy},
        {"gradientTransform", PropertyID::GradientTransform},
        {"gradientUnits", PropertyID::GradientUnits},
        {"height", PropertyID::Height},
        {"href", PropertyID::Href},
        {"id", PropertyID::Id},
        {"in", PropertyID::In},
        {"in2", PropertyID::In2},
        {"k1", PropertyID::K1},
        {"k2", PropertyID::K2},
        {"k3", PropertyID::K3},
        {"k4", PropertyID::K4},
        {"lengthAdjust", PropertyID::LengthAdjust},
        {"markerHeight", PropertyID::MarkerHeight},
        {"markerUnits", PropertyID::MarkerUnits},
        {"markerWidth", PropertyID::MarkerWidth},
        {"maskContentUnits", PropertyID::MaskContentUnits},
        {"maskUnits", PropertyID::MaskUnits},
        {"mode", PropertyID::Mode},
        {"offset", PropertyID::Offset},
        {"operator", PropertyID::Operator},
        {"orient", PropertyID::Orient},
        {"patternContentUnits", PropertyID::PatternContentUnits},
        {"patternTransform", PropertyID::PatternTransform},
        {"patternUnits", PropertyID::PatternUnits},
        {"points", PropertyID::Points},
        {"preserveAspectRatio", PropertyID::PreserveAspectRatio},
        {"primitiveUnits", PropertyID::PrimitiveUnits},
        {"r", PropertyID::R},
        {"refX", PropertyID::RefX},
        {"refY", PropertyID::RefY},
        {"result", PropertyID::Result},
        {"rotate", PropertyID::Rotate},
        {"rx", PropertyID::Rx},
        {"ry", PropertyID::Ry},
        {"spreadMethod", PropertyID::SpreadMethod},
        {"stdDeviation", PropertyID::StdDeviation},
        {"style", PropertyID::Style},
        {"textLength", PropertyID::TextLength},
        {"transform", PropertyID::Transform},
        {"type", PropertyID::Type},
        {"values", PropertyID::Values},
        {"viewBox", PropertyID::ViewBox},
        {"width", PropertyID::Width},
        {"x", PropertyID::X},
        {"x1", PropertyID::X1},
        {"x2", PropertyID::X2},
        {"xlink:href", PropertyID::Href},
        {"xml:space", PropertyID::White_Space},
        {"y", PropertyID::Y},
        {"y1", PropertyID::Y1},
        {"y2", PropertyID::Y2}
    };

    auto it = std::lower_bound(table, std::end(table), name, [](const auto& item, const auto& name) { return item.name < name; });
    if(it == std::end(table) || it->name != name)
        return csspropertyid(name);
    return it->value;
}

PropertyID csspropertyid(const std::string_view& name)
{
    static const struct {
        std::string_view name;
        PropertyID value;
    } table[] = {
        {"alignment-baseline", PropertyID::Alignment_Baseline},
        {"baseline-shift", PropertyID::Baseline_Shift},
        {"clip-path", PropertyID::Clip_Path},
        {"clip-rule", PropertyID::Clip_Rule},
        {"color", PropertyID::Color},
        {"cx", PropertyID::Cx},
        {"cy", PropertyID::Cy},
        {"d", PropertyID::D},
        {"direction", PropertyID::Direction},
        {"display", PropertyID::Display},
        {"dominant-baseline", PropertyID::Dominant_Baseline},
        {"dx", PropertyID::Dx},
        {"dy", PropertyID::Dy},
        {"fill", PropertyID::Fill},
        {"fill-opacity", PropertyID::Fill_Opacity},
        {"fill-rule", PropertyID::Fill_Rule},
        {"filter", PropertyID::Filter},
        {"flood-color", PropertyID::Flood_Color},
        {"flood-opacity", PropertyID::Flood_Opacity},
        {"font-family", PropertyID::Font_Family},
        {"font-size", PropertyID::Font_Size},
        {"font-style", PropertyID::Font_Style},
        {"font-weight", PropertyID::Font_Weight},
        {"fx", PropertyID::Fx},
        {"fy", PropertyID::Fy},
        {"gradientTransform", PropertyID::GradientTransform},
        {"gradientUnits", PropertyID::GradientUnits},
        {"height", PropertyID::Height},
        {"href", PropertyID::Href},
        {"id", PropertyID::Id},
        {"in", PropertyID::In},
        {"lengthAdjust", PropertyID::LengthAdjust},
        {"letter-spacing", PropertyID::Letter_Spacing},
        {"marker-end", PropertyID::Marker_End},
        {"marker-mid", PropertyID::Marker_Mid},
        {"marker-start", PropertyID::Marker_Start},
        {"markerHeight", PropertyID::MarkerHeight},
        {"markerUnits", PropertyID::MarkerUnits},
        {"markerWidth", PropertyID::MarkerWidth},
        {"mask", PropertyID::Mask},
        {"mask-type", PropertyID::Mask_Type},
        {"maskContentUnits", PropertyID::MaskContentUnits},
        {"maskUnits", PropertyID::MaskUnits},
        {"offset", PropertyID::Offset},
        {"opacity", PropertyID::Opacity},
        {"orient", PropertyID::Orient},
        {"overflow", PropertyID::Overflow},
        {"patternContentUnits", PropertyID::PatternContentUnits},
        {"patternTransform", PropertyID::PatternTransform},
        {"patternUnits", PropertyID::PatternUnits},
        {"pointer-events", PropertyID::Pointer_Events},
        {"points", PropertyID::Points},
        {"preserveAspectRatio", PropertyID::PreserveAspectRatio},
        {"primitiveUnits", PropertyID::PrimitiveUnits},
        {"r", PropertyID::R},
        {"refX", PropertyID::RefX},
        {"refY", PropertyID::RefY},
        {"result", PropertyID::Result},
        {"rotate", PropertyID::Rotate},
        {"rx", PropertyID::Rx},
        {"ry", PropertyID::Ry},
        {"spreadMethod", PropertyID::SpreadMethod},
        {"stdDeviation", PropertyID::StdDeviation},
        {"stop-color", PropertyID::Stop_Color},
        {"stop-opacity", PropertyID::Stop_Opacity},
        {"stroke", PropertyID::Stroke},
        {"stroke-dasharray", PropertyID::Stroke_Dasharray},
        {"stroke-dashoffset", PropertyID::Stroke_Dashoffset},
        {"stroke-linecap", PropertyID::Stroke_Linecap},
        {"stroke-linejoin", PropertyID::Stroke_Linejoin},
        {"stroke-miterlimit", PropertyID::Stroke_Miterlimit},
        {"stroke-opacity", PropertyID::Stroke_Opacity},
        {"stroke-width", PropertyID::Stroke_Width},
        {"text-anchor", PropertyID::Text_Anchor},
        {"text-orientation", PropertyID::Text_Orientation},
        {"visibility", PropertyID::Visibility},
        {"white-space", PropertyID::White_Space},
        {"word-spacing", PropertyID::Word_Spacing},
        {"writing-mode", PropertyID::Writing_Mode}
    };

    auto it = std::lower_bound(table, std::end(table), name, [](const auto& item, const auto& name) { return item.name < name; });
    if(it == std::end(table) || it->name != name)
        return PropertyID::Unknown;
    return it->value;
}

SVGProperty::SVGProperty(PropertyID id)
    : m_id(id)
{
}

bool SVGString::parse(std::string_view input)
{
    stripLeadingAndTrailingSpaces(input);
    m_value.assign(input);
    return true;
}

template<>
bool SVGEnumeration<SpreadMethod>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<SpreadMethod> entries[] = {
        {SpreadMethod::Pad, "pad"},
        {SpreadMethod::Reflect, "reflect"},
        {SpreadMethod::Repeat, "repeat"}
    };

    return parseEnum(input, entries);
}

template<>
bool SVGEnumeration<Units>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<Units> entries[] = {
        {Units::UserSpaceOnUse, "userSpaceOnUse"},
        {Units::ObjectBoundingBox, "objectBoundingBox"}
    };

    return parseEnum(input, entries);
}

template<>
bool SVGEnumeration<MarkerUnits>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<MarkerUnits> entries[] = {
        {MarkerUnits::StrokeWidth, "strokeWidth"},
        {MarkerUnits::UserSpaceOnUse, "userSpaceOnUse"}
    };

    return parseEnum(input, entries);
}

template<>
bool SVGEnumeration<LengthAdjust>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<LengthAdjust> entries[] = {
        {LengthAdjust::Spacing, "spacing"},
        {LengthAdjust::SpacingAndGlyphs, "spacingAndGlyphs"}
    };

    return parseEnum(input, entries);
}

template<>
bool SVGEnumeration<FEBlendMode>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<FEBlendMode> entries[] = {
        {FEBlendMode::Normal, "normal"},
        {FEBlendMode::Multiply, "multiply"},
        {FEBlendMode::Screen, "screen"},
        {FEBlendMode::Darken, "darken"},
        {FEBlendMode::Lighten, "lighten"}
    };

    return parseEnum(input, entries);
}

template<>
bool SVGEnumeration<FECompositeOperator>::parse(std::string_view input)
{
    static const SVGEnumerationEntry<FECompositeOperator> entries[] = {
        {FECompositeOperator::Over, "over"},
        {FECompositeOperator::In, "in"},
        {FECompositeOperator::Out, "out"},
        {FECompositeOperator::Atop, "atop"},
        {FECompositeOperator::Xor, "xor"},
        {FECompositeOperator::Arithmetic, "arithmetic"}
    };

    return parseEnum(input, entries);
}

bool SVGAngle::parse(std::string_view input)
{
    stripLeadingAndTrailingSpaces(input);
    if(input == "auto") {
        m_value = 0.f;
        m_orientType = OrientType::Auto;
        return true;
    }

    if(input == "auto-start-reverse") {
        m_value = 0.f;
        m_orientType = OrientType::AutoStartReverse;
        return true;
    }

    float value = 0.f;
    if(!parseNumber(input, value))
        return false;
    if(!input.empty()) {
        if(input == "rad")
            value *= 180.f / PLUTOVG_PI;
        else if(input == "grad")
            value *= 360.f / 400.f;
        else if(input == "turn")
            value *= 360.f;
        else if(input != "deg") {
            return false;
        }
    }

    m_value = value;
    m_orientType = OrientType::Angle;
    return true;
}

bool Length::parse(std::string_view input, LengthNegativeMode mode)
{
    float value = 0.f;
    stripLeadingAndTrailingSpaces(input);
    if(!parseNumber(input, value))
        return false;
    if(value < 0.f && mode == LengthNegativeMode::Forbid)
        return false;
    if(input.empty()) {
        m_value = value;
        m_units = LengthUnits::None;
        return true;
    }

    constexpr auto dpi = 96.f;
    switch(input.front()) {
    case '%':
        m_value = value;
        m_units = LengthUnits::Percent;
        input.remove_prefix(1);
        break;
    case 'p':
        input.remove_prefix(1);
        if(input.empty())
            return false;
        else if(input.front() == 'x')
            m_value = value;
        else if(input.front() == 'c')
            m_value = value * dpi / 6.f;
        else if(input.front() == 't')
            m_value = value * dpi / 72.f;
        else
            return false;
        m_units = LengthUnits::Px;
        input.remove_prefix(1);
        break;
    case 'i':
        input.remove_prefix(1);
        if(input.empty())
            return false;
        else if(input.front() == 'n')
            m_value = value * dpi;
        else
            return false;
        m_units = LengthUnits::Px;
        input.remove_prefix(1);
        break;
    case 'c':
        input.remove_prefix(1);
        if(input.empty())
            return false;
        else if(input.front() == 'm')
            m_value = value * dpi / 2.54f;
        else
            return false;
        m_units = LengthUnits::Px;
        input.remove_prefix(1);
        break;
    case 'm':
        input.remove_prefix(1);
        if(input.empty())
            return false;
        else if(input.front() == 'm')
            m_value = value * dpi / 25.4f;
        else
            return false;
        m_units = LengthUnits::Px;
        input.remove_prefix(1);
        break;
    case 'e':
        input.remove_prefix(1);
        if(input.empty())
            return false;
        else if(input.front() == 'm')
            m_units = LengthUnits::Em;
        else if(input.front() == 'x')
            m_units = LengthUnits::Ex;
        else
            return false;
        m_value = value;
        input.remove_prefix(1);
        break;
    default:
        return false;
    }

    return input.empty();
}

float LengthContext::valueForLength(const Length& length, LengthDirection direction) const
{
    if(length.units() == LengthUnits::Percent) {
        if(m_units == Units::UserSpaceOnUse)
            return length.value() * viewportDimension(direction) / 100.f;
        return length.value() / 100.f;
    }

    if(length.units() == LengthUnits::Ex)
        return length.value() * m_element->font_size() / 2.f;
    if(length.units() == LengthUnits::Em)
        return length.value() * m_element->font_size();
    return length.value();
}

float LengthContext::viewportDimension(LengthDirection direction) const
{
    auto viewportSize = m_element->currentViewportSize();
    switch(direction) {
    case LengthDirection::Horizontal:
        return viewportSize.w;
    case LengthDirection::Vertical:
        return viewportSize.h;
    default:
        return std::sqrt(viewportSize.w * viewportSize.w + viewportSize.h * viewportSize.h) / PLUTOVG_SQRT2;
    }
}

bool SVGLength::parse(std::string_view input)
{
    return m_value.parse(input, m_negativeMode);
}

bool SVGLengthList::parse(std::string_view input)
{
    m_values.clear();
    while(!input.empty()) {
        size_t count = 0;
        while(count < input.length() && input[count] != ',' && !IS_WS(input[count]))
            ++count;
        if(count == 0)
            break;
        Length value(0, LengthUnits::None);
        if(!value.parse(input.substr(0, count), m_negativeMode))
            return false;
        input.remove_prefix(count);
        skipOptionalSpacesOrComma(input);
        m_values.push_back(value);
    }

    return true;
}

bool SVGNumber::parse(std::string_view input)
{
    float value = 0.f;
    stripLeadingAndTrailingSpaces(input);
    if(!parseNumber(input, value))
        return false;
    if(!input.empty())
        return false;
    m_value = value;
    return true;
}

bool SVGNumberPercentage::parse(std::string_view input)
{
    float value = 0.f;
    stripLeadingAndTrailingSpaces(input);
    if(!parseNumber(input, value))
        return false;
    if(!input.empty() && input.front() == '%') {
        value /= 100.f;
        input.remove_prefix(1);
    }

    if(!input.empty())
        return false;
    m_value = std::clamp(value, 0.f, 1.f);
    return true;
}

bool SVGNumberList::parse(std::string_view input)
{
    m_values.clear();
    stripLeadingSpaces(input);
    while(!input.empty()) {
        float value = 0.f;
        if(!parseNumber(input, value))
            return false;
        skipOptionalSpacesOrComma(input);
        m_values.push_back(value);
    }

    return true;
}

bool SVGPath::parse(std::string_view input)
{
    return m_value.parse(input.data(), input.length());
}

bool SVGPoint::parse(std::string_view input)
{
    Point value;
    stripLeadingAndTrailingSpaces(input);
    if(!parseNumber(input, value.x)
        || !skipOptionalSpaces(input)
        || !parseNumber(input, value.y)
        || !input.empty()) {
        return false;
    }

    m_value = value;
    return true;
}

bool SVGPointList::parse(std::string_view input)
{
    m_values.clear();
    stripLeadingSpaces(input);
    while(!input.empty()) {
        Point value;
        if(!parseNumber(input, value.x)
            || !skipOptionalSpacesOrComma(input)
            || !parseNumber(input, value.y)) {
            return false;
        }

        m_values.push_back(value);
        skipOptionalSpacesOrComma(input);
    }

    return true;
}

bool SVGRect::parse(std::string_view input)
{
    Rect value;
    stripLeadingAndTrailingSpaces(input);
    if(!parseNumber(input, value.x)
        || !skipOptionalSpacesOrComma(input)
        || !parseNumber(input, value.y)
        || !skipOptionalSpacesOrComma(input)
        || !parseNumber(input, value.w)
        || !skipOptionalSpacesOrComma(input)
        || !parseNumber(input, value.h)
        || !input.empty()) {
        return false;
    }

    if(value.w < 0.f || value.h < 0.f)
        return false;
    m_value = value;
    return true;
}

bool SVGTransform::parse(std::string_view input)
{
    return m_value.parse(input.data(), input.length());
}

bool SVGPreserveAspectRatio::parse(std::string_view input)
{
    auto alignType = AlignType::xMidYMid;
    stripLeadingSpaces(input);
    if(skipString(input, "none"))
        alignType = AlignType::None;
    else if(skipString(input, "xMinYMin"))
        alignType = AlignType::xMinYMin;
    else if(skipString(input, "xMidYMin"))
        alignType = AlignType::xMidYMin;
    else if(skipString(input, "xMaxYMin"))
        alignType = AlignType::xMaxYMin;
    else if(skipString(input, "xMinYMid"))
        alignType = AlignType::xMinYMid;
    else if(skipString(input, "xMidYMid"))
        alignType = AlignType::xMidYMid;
    else if(skipString(input, "xMaxYMid"))
        alignType = AlignType::xMaxYMid;
    else if(skipString(input, "xMinYMax"))
        alignType = AlignType::xMinYMax;
    else if(skipString(input, "xMidYMax"))
        alignType = AlignType::xMidYMax;
    else if(skipString(input, "xMaxYMax"))
        alignType = AlignType::xMaxYMax;
    else {
        return false;
    }

    auto meetOrSlice = MeetOrSlice::Meet;
    skipOptionalSpaces(input);
    if(skipString(input, "meet")) {
        meetOrSlice = MeetOrSlice::Meet;
    } else if(skipString(input, "slice")) {
        meetOrSlice = MeetOrSlice::Slice;
    }

    if(alignType == AlignType::None)
        meetOrSlice = MeetOrSlice::Meet;
    skipOptionalSpaces(input);
    if(!input.empty())
        return false;
    m_alignType = alignType;
    m_meetOrSlice = meetOrSlice;
    return true;
}

Rect SVGPreserveAspectRatio::getClipRect(const Rect& viewBoxRect, const Size& viewportSize) const    
{
    assert(!viewBoxRect.isEmpty() && !viewportSize.isEmpty());
    auto xScale = viewportSize.w / viewBoxRect.w;
    auto yScale = viewportSize.h / viewBoxRect.h;
    if(m_alignType == AlignType::None) {
        return Rect(viewBoxRect.x, viewBoxRect.y, viewportSize.w / xScale, viewportSize.h / yScale); 
    }

    auto scale = (m_meetOrSlice == MeetOrSlice::Meet) ? std::min(xScale, yScale) : std::max(xScale, yScale);
    auto xOffset = -viewBoxRect.x * scale;
    auto yOffset = -viewBoxRect.y * scale;
    auto viewWidth = viewBoxRect.w * scale;
    auto viewHeight = viewBoxRect.h * scale;

    if(m_alignType == AlignType::xMidYMin || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMidYMax)
        xOffset += (viewportSize.w - viewWidth) * 0.5f;
    else if(m_alignType == AlignType::xMaxYMin || m_alignType == AlignType::xMaxYMid || m_alignType == AlignType::xMaxYMax)
        xOffset += (viewportSize.w - viewWidth);

    if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
        yOffset += (viewportSize.h - viewHeight) * 0.5f;
    else if(m_alignType == AlignType::xMinYMax || m_alignType == AlignType::xMidYMax || m_alignType == AlignType::xMaxYMax)
        yOffset += (viewportSize.h - viewHeight);

    return Rect(-xOffset / scale, -yOffset / scale, viewportSize.w / scale, viewportSize.h / scale); 
}

Transform SVGPreserveAspectRatio::getTransform(const Rect& viewBoxRect, const Size& viewportSize) const
{
    assert(!viewBoxRect.isEmpty() && !viewportSize.isEmpty());
    auto xScale = viewportSize.w / viewBoxRect.w;
    auto yScale = viewportSize.h / viewBoxRect.h;
    if(m_alignType == AlignType::None) {
        return Transform(xScale, 0, 0, yScale, -viewBoxRect.x * xScale, -viewBoxRect.y * yScale);    
    }

    auto scale = (m_meetOrSlice == MeetOrSlice::Meet) ? std::min(xScale, yScale) : std::max(xScale, yScale);
    auto xOffset = -viewBoxRect.x * scale;
    auto yOffset = -viewBoxRect.y * scale;
    auto viewWidth = viewBoxRect.w * scale;
    auto viewHeight = viewBoxRect.h * scale;

    if(m_alignType == AlignType::xMidYMin || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMidYMax)
        xOffset += (viewportSize.w - viewWidth) * 0.5f;
    else if(m_alignType == AlignType::xMaxYMin || m_alignType == AlignType::xMaxYMid || m_alignType == AlignType::xMaxYMax)
        xOffset += (viewportSize.w - viewWidth);

    if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
        yOffset += (viewportSize.h - viewHeight) * 0.5f;
    else if(m_alignType == AlignType::xMinYMax || m_alignType == AlignType::xMidYMax || m_alignType == AlignType::xMaxYMax)
        yOffset += (viewportSize.h - viewHeight);

    return Transform(scale, 0, 0, scale, xOffset, yOffset);
}

void SVGPreserveAspectRatio::transformRect(Rect& dstRect, Rect& srcRect) const
{
    if(m_alignType == AlignType::None)
        return;
    auto viewSize = dstRect.size();
    auto imageSize = srcRect.size();
    if(m_meetOrSlice == MeetOrSlice::Meet) {
        auto scale = imageSize.h / imageSize.w;
        if(viewSize.h > viewSize.w * scale) {
            dstRect.h = viewSize.w * scale;
            if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
                dstRect.y += (viewSize.h - dstRect.h) * 0.5f;
            else if(m_alignType == AlignType::xMinYMax || m_alignType == AlignType::xMidYMax || m_alignType == AlignType::xMaxYMax)
                dstRect.y += viewSize.h - dstRect.h;
        }

        if(viewSize.w > viewSize.h / scale) {
            dstRect.w = viewSize.h / scale;
            if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
                dstRect.x += (viewSize.w - dstRect.w) * 0.5f;
            else if(m_alignType == AlignType::xMaxYMin || m_alignType == AlignType::xMaxYMid || m_alignType == AlignType::xMaxYMax)
                dstRect.x += viewSize.w - dstRect.w;
        }
    } else if(m_meetOrSlice == MeetOrSlice::Slice) {
        auto scale = imageSize.h / imageSize.w;
        if(viewSize.h < viewSize.w * scale) {
            srcRect.h = viewSize.h * (imageSize.w / viewSize.w);
            if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
                srcRect.y += (imageSize.h - srcRect.h) * 0.5f;
            else if(m_alignType == AlignType::xMinYMax || m_alignType == AlignType::xMidYMax || m_alignType == AlignType::xMaxYMax)
                srcRect.y += imageSize.h - srcRect.h;
        }

        if(viewSize.w < viewSize.h / scale) {
            srcRect.w = viewSize.w * (imageSize.h / viewSize.h);
            if(m_alignType == AlignType::xMinYMid || m_alignType == AlignType::xMidYMid || m_alignType == AlignType::xMaxYMid)
                srcRect.x += (imageSize.w - srcRect.w) * 0.5f;
            else if(m_alignType == AlignType::xMaxYMin || m_alignType == AlignType::xMaxYMid || m_alignType == AlignType::xMaxYMax)
                srcRect.x += imageSize.w - srcRect.w;
        }
    }
}

} // namespace lunasvg