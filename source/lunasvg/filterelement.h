#ifndef LUNASVG_FILTERELEMENT_H
#define LUNASVG_FILTERELEMENT_H

#include "element.h"
#include <map>
#include <vector>

namespace lunasvg {

struct FilterPixel {
    float r, g, b, a;
};

class FilterImage {
public:
    FilterImage(int w, int h);
    static std::shared_ptr<FilterImage> fromCanvas(const Canvas& canvas);
    std::shared_ptr<Canvas> toCanvas(const Rect& extents) const;

    int width() const { return m_width; }
    int height() const { return m_height; }
    FilterPixel* data() { return m_pixels.data(); }
    const FilterPixel* data() const { return m_pixels.data(); }

private:
    int m_width;
    int m_height;
    std::vector<FilterPixel> m_pixels;
};

class FilterContext {
public:
    FilterContext(const SVGFilterElement* filter, const SVGElement* element, const SVGRenderState& state, const Canvas& sourceGraphic);

    std::shared_ptr<FilterImage> getInput(const std::string& in);
    void addResult(const std::string& result, std::shared_ptr<FilterImage> image);

    const SVGFilterElement* filter;
    const SVGElement* element;
    const SVGRenderState& state;
    std::shared_ptr<FilterImage> sourceGraphic;
    std::shared_ptr<FilterImage> sourceAlpha;
    std::map<std::string, std::shared_ptr<FilterImage>, std::less<>> results;
    std::shared_ptr<FilterImage> lastResult;
};

class SVGFilterElement final : public SVGElement {
public:
    SVGFilterElement(Document* document);

    const SVGLength& x() const { return m_x; }
    const SVGLength& y() const { return m_y; }
    const SVGLength& width() const { return m_width; }
    const SVGLength& height() const { return m_height; }
    const SVGEnumeration<Units>& filterUnits() const { return m_filterUnits; }
    const SVGEnumeration<Units>& primitiveUnits() const { return m_primitiveUnits; }

    std::shared_ptr<Canvas> applyFilter(const SVGRenderState& state, const Canvas& sourceGraphic) const;

private:
    SVGLength m_x;
    SVGLength m_y;
    SVGLength m_width;
    SVGLength m_height;
    SVGEnumeration<Units> m_filterUnits;
    SVGEnumeration<Units> m_primitiveUnits;
};

class SVGFilterPrimitiveElement : public SVGElement {
public:
    SVGFilterPrimitiveElement(Document* document, ElementID id);

    const SVGString& in() const { return m_in; }
    const SVGString& result() const { return m_result; }
    const SVGLength& x() const { return m_x; }
    const SVGLength& y() const { return m_y; }
    const SVGLength& width() const { return m_width; }
    const SVGLength& height() const { return m_height; }

    bool isFilterPrimitiveElement() const final { return true; }
    virtual void render(FilterContext& context) const = 0;

private:
    SVGString m_in;
    SVGString m_result;
    SVGLength m_x;
    SVGLength m_y;
    SVGLength m_width;
    SVGLength m_height;
};

class SVGFeGaussianBlurElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeGaussianBlurElement(Document* document);

    const SVGNumberList& stdDeviation() const { return m_stdDeviation; }

    void render(FilterContext& context) const final;

private:
    SVGNumberList m_stdDeviation;
};

class SVGFeOffsetElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeOffsetElement(Document* document);

    const SVGNumber& dx() const { return m_dx; }
    const SVGNumber& dy() const { return m_dy; }

    void render(FilterContext& context) const final;

private:
    SVGNumber m_dx;
    SVGNumber m_dy;
};

class SVGFeMergeNodeElement final : public SVGElement {
public:
    SVGFeMergeNodeElement(Document* document);

    const SVGString& in() const { return m_in; }

private:
    SVGString m_in;
};

class SVGFeMergeElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeMergeElement(Document* document);

    void render(FilterContext& context) const final;
};

class SVGFeFloodElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeFloodElement(Document* document);

    void layoutElement(const SVGLayoutState& state) final;
    void render(FilterContext& context) const final;

private:
    Color m_flood_color = Color::Black;
    float m_flood_opacity = 1.f;
};

class SVGFeBlendElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeBlendElement(Document* document);

    const SVGString& in2() const { return m_in2; }
    const SVGEnumeration<FEBlendMode>& mode() const { return m_mode; }

    void render(FilterContext& context) const final;

private:
    SVGString m_in2;
    SVGEnumeration<FEBlendMode> m_mode;
};

class SVGFeCompositeElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeCompositeElement(Document* document);

    const SVGString& in2() const { return m_in2; }
    const SVGEnumeration<FECompositeOperator>& _operator() const { return m_operator; }
    const SVGNumber& k1() const { return m_k1; }
    const SVGNumber& k2() const { return m_k2; }
    const SVGNumber& k3() const { return m_k3; }
    const SVGNumber& k4() const { return m_k4; }

    void render(FilterContext& context) const final;

private:
    SVGString m_in2;
    SVGEnumeration<FECompositeOperator> m_operator;
    SVGNumber m_k1;
    SVGNumber m_k2;
    SVGNumber m_k3;
    SVGNumber m_k4;
};

class SVGFeColorMatrixElement final : public SVGFilterPrimitiveElement {
public:
    SVGFeColorMatrixElement(Document* document);

    const SVGEnumeration<ColorMatrixType>& type() const { return m_type; }
    const SVGNumberList& values() const { return m_values; }

    void render(FilterContext& context) const final;

private:
    SVGEnumeration<ColorMatrixType> m_type;
    SVGNumberList m_values;
};

} // namespace lunasvg

#endif // LUNASVG_FILTERELEMENT_H