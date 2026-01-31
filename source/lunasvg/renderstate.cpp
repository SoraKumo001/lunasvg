#include "renderstate.h"
#include "filterelement.h"
#include "property.h"

namespace lunasvg {

SVGBlendInfo::SVGBlendInfo(const SVGElement* element)
    : m_clipper(element->clipper())
    , m_masker(element->masker())
    , m_filter(element->filter())
    , m_opacity(element->opacity())
{
}

bool SVGBlendInfo::requiresCompositing(SVGRenderMode mode) const
{
    return (m_clipper && m_clipper->requiresMasking()) || (mode == SVGRenderMode::Painting && (m_masker || m_filter || m_opacity < 1.f));  
}

bool SVGRenderState::hasCycleReference(const SVGElement* element) const
{
    auto current = this;
    do {
        if(element == current->element())
            return true;
        current = current->parent();
    } while(current);
    return false;
}

void SVGRenderState::beginGroup(const SVGBlendInfo& blendInfo)
{
    auto requiresCompositing = blendInfo.requiresCompositing(m_mode);
    if(requiresCompositing) {
        auto boundingBox = m_currentTransform.mapRect(m_element->paintBoundingBox());
        if(auto filter = blendInfo.filter()) {
            auto filterUnits = filter->filterUnits().value();
            LengthContext context(filter, filterUnits);
            auto x = context.valueForLength(filter->x());
            auto y = context.valueForLength(filter->y());
            auto w = context.valueForLength(filter->width());
            auto h = context.valueForLength(filter->height());

            Rect filterRect;
            if(filterUnits == Units::ObjectBoundingBox) {
                auto bbox = m_element->paintBoundingBox();
                filterRect = Rect(bbox.x + x * bbox.w, bbox.y + y * bbox.h, w * bbox.w, h * bbox.h);
            } else {
                filterRect = Rect(x, y, w, h);
            }
            boundingBox = m_currentTransform.mapRect(filterRect);
        }
        boundingBox.intersect(m_canvas->extents());
        m_canvas = Canvas::create(boundingBox);
    } else {
        m_canvas->save();
    }

    if(!requiresCompositing && blendInfo.clipper()) {
        blendInfo.clipper()->applyClipPath(*this);
    }
}

void SVGRenderState::endGroup(const SVGBlendInfo& blendInfo)
{
    if(m_canvas == m_parent->canvas()) {
        m_canvas->restore();
        return;
    }

    auto opacity = m_mode == SVGRenderMode::Clipping ? 1.f : blendInfo.opacity();
    if(m_mode == SVGRenderMode::Painting && blendInfo.filter()) {
        if(auto canvas = blendInfo.filter()->applyFilter(*this, *m_canvas)) {
            m_canvas = std::move(canvas);
        }
    }

    if(blendInfo.clipper())
        blendInfo.clipper()->applyClipMask(*this);
    if(m_mode == SVGRenderMode::Painting && blendInfo.masker()) {
        blendInfo.masker()->applyMask(*this);
    }

    m_parent->m_canvas->blendCanvas(*m_canvas, BlendMode::Src_Over, opacity);
}

} // namespace lunasvg
