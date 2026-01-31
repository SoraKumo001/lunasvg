#include "lunasvg.h"
#include "element.h"
#include "renderstate.h"

#include <cstring>
#include <fstream>
#include <cmath>

int lunasvg_version()
{
    return LUNASVG_VERSION;
}

const char* lunasvg_version_string()
{
    return LUNASVG_VERSION_STRING;
}

bool lunasvg_add_font_face_from_file(const char* family, bool bold, bool italic, const char* filename)
{
    return plutovg_canvas_add_font_file(nullptr, family, bold, italic, filename, 0);
}

bool lunasvg_add_font_face_from_data(const char* family, bool bold, bool italic, const void* data, size_t length, lunasvg_destroy_func_t destroy_func, void* closure)
{
    auto face = plutovg_font_face_load_from_data((const unsigned char*)data, (unsigned int)length, 0, (plutovg_destroy_func_t)destroy_func, closure);
    if(face == nullptr)
        return false;
    plutovg_canvas_add_font_face(nullptr, family, bold, italic, face);
    plutovg_font_face_destroy(face);
    return true;
}

namespace lunasvg {

Bitmap::Bitmap(int width, int height)
    : m_surface(plutovg_surface_create(width, height))
{
}

Bitmap::Bitmap(uint8_t* data, int width, int height, int stride)
    : m_surface(plutovg_surface_create_for_data(data, width, height, stride))
{
}

Bitmap::Bitmap(const Bitmap& bitmap)
    : m_surface(plutovg_surface_reference(bitmap.m_surface))
{
}

Bitmap::Bitmap(Bitmap&& bitmap)
    : m_surface(bitmap.release())
{
}

Bitmap::~Bitmap()
{
    plutovg_surface_destroy(m_surface);
}

Bitmap& Bitmap::operator=(const Bitmap& bitmap)
{
    if(this != &bitmap) {
        plutovg_surface_t* surface = plutovg_surface_reference(bitmap.m_surface);
        plutovg_surface_destroy(m_surface);
        m_surface = surface;
    }

    return *this;
}

Bitmap& Bitmap::operator=(Bitmap&& bitmap)
{
    if(this != &bitmap) {
        plutovg_surface_destroy(m_surface);
        m_surface = bitmap.release();
    }

    return *this;
}

void Bitmap::swap(Bitmap& bitmap)
{
    std::swap(m_surface, bitmap.m_surface);
}

uint8_t* Bitmap::data() const
{
    return plutovg_surface_get_data(m_surface);
}

int Bitmap::width() const
{
    return plutovg_surface_get_width(m_surface);
}

int Bitmap::height() const
{
    return plutovg_surface_get_height(m_surface);
}

int Bitmap::stride() const
{
    return plutovg_surface_get_stride(m_surface);
}

void Bitmap::clear(uint32_t value)
{
    plutovg_color_t color;
    plutovg_color_init_rgba32(&color, value);
    plutovg_surface_clear(m_surface, &color);
}

void Bitmap::convertToRGBA()
{
    plutovg_convert_argb_to_rgba(data(), data(), width(), height(), stride());
}

bool Bitmap::writeToPng(const std::string& filename) const
{
    return plutovg_surface_write_to_png(m_surface, filename.data());
}

bool Bitmap::writeToPng(lunasvg_write_func_t callback, void* closure) const
{
    return plutovg_surface_write_to_png_stream(m_surface, (plutovg_write_func_t)callback, closure);
}

plutovg_surface_t* Bitmap::release()
{
    return std::exchange(m_surface, nullptr);
}

Box::Box(float x, float y, float w, float h)
    : x(x), y(y), w(w), h(h)
{
}

Box::Box(const Rect& rect)
    : x(rect.x), y(rect.y), w(rect.w), h(rect.h)
{
}

Box& Box::transform(const Matrix& matrix)
{
    *this = transformed(matrix);
    return *this;
}

Box Box::transformed(const Matrix& matrix) const
{
    Rect rect(x, y, w, h);
    return Box(Transform(matrix).mapRect(rect));
}

Matrix::Matrix(float a, float b, float c, float d, float e, float f)
    : a(a), b(b), c(c), d(d), e(e), f(f)
{
}

Matrix::Matrix(const plutovg_matrix_t& matrix)
    : a(matrix.a), b(matrix.b), c(matrix.c), d(matrix.d), e(matrix.e), f(matrix.f)
{
}

Matrix::Matrix(const Transform& transform)
    : Matrix(transform.matrix())
{
}

Matrix Matrix::operator*(const Matrix& matrix) const
{
    return Matrix(Transform(*this) * Transform(matrix));
}

Matrix& Matrix::operator*=(const Matrix& matrix)
{
    return multiply(matrix);
}

Matrix& Matrix::multiply(const Matrix& matrix)
{
    Transform t(*this);
    t.multiply(Transform(matrix));
    *this = Matrix(t);
    return *this;
}

Matrix& Matrix::translate(float tx, float ty)
{
    Transform t(*this);
    t.translate(tx, ty);
    *this = Matrix(t);
    return *this;
}

Matrix& Matrix::scale(float sx, float sy)
{
    Transform t(*this);
    t.scale(sx, sy);
    *this = Matrix(t);
    return *this;
}

Matrix& Matrix::rotate(float angle, float cx, float cy)
{
    Transform t(*this);
    t.rotate(angle, cx, cy);
    *this = Matrix(t);
    return *this;
}

Matrix& Matrix::shear(float shx, float shy)
{
    Transform t(*this);
    t.shear(shx, shy);
    *this = Matrix(t);
    return *this;
}

Matrix& Matrix::invert()
{
    Transform t(*this);
    t.invert();
    *this = Matrix(t);
    return *this;
}

Matrix Matrix::inverse() const
{
    return Matrix(Transform(*this).inverse());
}

void Matrix::reset()
{
    *this = Matrix();
}

Matrix Matrix::translated(float tx, float ty)
{
    return Matrix(Transform::translated(tx, ty));
}

Matrix Matrix::scaled(float sx, float sy)
{
    return Matrix(Transform::scaled(sx, sy));
}

Matrix Matrix::rotated(float angle, float cx, float cy)
{
    return Matrix(Transform::rotated(angle, cx, cy));
}

Matrix Matrix::sheared(float shx, float shy)
{
    return Matrix(Transform::sheared(shx, shy));
}

Node::Node(SVGNode* node)
    : m_node(node)
{
}

bool Node::isTextNode() const
{
    return m_node && m_node->isTextNode();
}

bool Node::isElement() const
{
    return m_node && m_node->isElement();
}

TextNode Node::toTextNode() const
{
    if(isTextNode())
        return TextNode(static_cast<SVGTextNode*>(m_node));
    return TextNode();
}

Element Node::toElement() const
{
    if(isElement())
        return Element(static_cast<SVGElement*>(m_node));
    return Element();
}

Element Node::parentElement() const
{
    if(m_node)
        return Element(m_node->parentElement());
    return Element();
}

TextNode::TextNode(SVGTextNode* text)
    : Node(text)
{
}

SVGTextNode* TextNode::text() const
{
    return static_cast<SVGTextNode*>(m_node);
}

const std::string& TextNode::data() const
{
    if(m_node)
        return text()->data();
    return emptyString;
}

void TextNode::setData(const std::string& data)
{
    if(m_node) {
        text()->setData(data);
    }
}

Element::Element(SVGElement* element)
    : Node(element)
{
}

SVGElement* Element::element(bool layoutIfNeeded) const
{
    auto element = static_cast<SVGElement*>(m_node);
    if(element && layoutIfNeeded) {
        element->rootElement()->layoutIfNeeded();
    }

    return element;
}

bool Element::hasAttribute(const std::string& name) const
{
    if(m_node)
        return element()->hasAttribute(name);
    return false;
}

const std::string& Element::getAttribute(const std::string& name) const
{
    if(m_node)
        return element()->getAttribute(name);
    return emptyString;
}

void Element::setAttribute(const std::string& name, const std::string& value)
{
    if(m_node) {
        element()->setAttribute(name, value);
    }
}

void Element::render(Bitmap& bitmap, const Matrix& matrix) const
{
    if(m_node && !bitmap.isNull()) {
        auto root = element(true)->rootElement();
        auto canvas = Canvas::create(bitmap);
        SVGRenderState state(root, nullptr, Transform(matrix), SVGRenderMode::Painting, canvas);
        element()->render(state);
    }
}

Bitmap Element::renderToBitmap(int width, int height, uint32_t backgroundColor) const
{
    if(m_node == nullptr)
        return Bitmap();

    auto root = element(true)->rootElement();
    if(width <= 0 && height <= 0) {
        width = (int)std::ceil(root->intrinsicWidth());
        height = (int)std::ceil(root->intrinsicHeight());
    } else if(width > 0 && height <= 0) {
        height = (int)std::ceil(width * root->intrinsicHeight() / root->intrinsicWidth());
    } else if(width <= 0 && height > 0) {
        width = (int)std::ceil(height * root->intrinsicWidth() / root->intrinsicHeight());
    }

    if(width <= 0 || height <= 0)
        return Bitmap();

    Bitmap bitmap(width, height);
    bitmap.clear(backgroundColor);
    render(bitmap, Matrix::scaled((float)width / root->intrinsicWidth(), (float)height / root->intrinsicHeight()));
    return bitmap;
}

Matrix Element::getLocalMatrix() const
{
    if(m_node)
        return Matrix(element()->localTransform());
    return Matrix();
}

Matrix Element::getGlobalMatrix() const
{
    if(m_node) {
        auto transform = element()->localTransform();
        for(auto parent = element()->parentElement(); parent; parent = parent->parentElement())
            transform.postMultiply(parent->localTransform());
        return Matrix(transform);
    }

    return Matrix();
}

Box Element::getLocalBoundingBox() const
{
    if(m_node)
        return Box(element()->localTransform().mapRect(element()->paintBoundingBox()));
    return Box();
}

Box Element::getGlobalBoundingBox() const
{
    if(m_node) {
        auto transform = element()->localTransform();
        for(auto parent = element()->parentElement(); parent; parent = parent->parentElement())
            transform.postMultiply(parent->localTransform());
        return Box(transform.mapRect(element()->paintBoundingBox()));
    }

    return Box();
}

Box Element::getBoundingBox() const
{
    if(m_node)
        return Box(element()->paintBoundingBox());
    return Box();
}

NodeList Element::children() const
{
    NodeList children;
    if(m_node) {
        for(auto& child : element()->children()) {
            children.push_back(Node(child.get()));
        }
    }

    return children;
}

std::unique_ptr<Document> Document::loadFromFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if(!file.is_open())
        return nullptr;

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadFromData(data);
}

std::unique_ptr<Document> Document::loadFromData(const std::string& string)
{
    return loadFromData(string.data(), string.size());
}

std::unique_ptr<Document> Document::loadFromData(const char* data)
{
    return loadFromData(data, std::strlen(data));
}

std::unique_ptr<Document> Document::loadFromData(const char* data, size_t length)
{
    std::unique_ptr<Document> document(new Document);
    if(!document->parse(data, length))
        return nullptr;
    return document;
}

float Document::width() const
{
    return rootElement(true)->intrinsicWidth();
}

float Document::height() const
{
    return rootElement(true)->intrinsicHeight();
}

Box Document::boundingBox() const
{
    auto root = rootElement(true);
    return Box(0, 0, root->intrinsicWidth(), root->intrinsicHeight());
}

void Document::updateLayout()
{
    rootElement(true);
}

void Document::forceLayout()
{
    m_rootElement->forceLayout();
}

void Document::render(Bitmap& bitmap, const Matrix& matrix) const
{
    if(!bitmap.isNull()) {
        auto root = rootElement(true);
        auto canvas = Canvas::create(bitmap);
        SVGRenderState state(root, nullptr, Transform(matrix), SVGRenderMode::Painting, canvas);
        root->render(state);
    }
}

Bitmap Document::renderToBitmap(int width, int height, uint32_t backgroundColor) const
{
    auto root = rootElement(true);
    if(width <= 0 && height <= 0) {
        width = (int)std::ceil(root->intrinsicWidth());
        height = (int)std::ceil(root->intrinsicHeight());
    } else if(width > 0 && height <= 0) {
        height = (int)std::ceil(width * root->intrinsicHeight() / root->intrinsicWidth());
    } else if(width <= 0 && height > 0) {
        width = (int)std::ceil(height * root->intrinsicWidth() / root->intrinsicHeight());
    }

    if(width <= 0 || height <= 0)
        return Bitmap();

    Bitmap bitmap(width, height);
    bitmap.clear(backgroundColor);
    render(bitmap, Matrix::scaled((float)width / root->intrinsicWidth(), (float)height / root->intrinsicHeight()));
    return bitmap;
}

Element Document::elementFromPoint(float x, float y) const
{
    return Element(rootElement(true)->elementFromPoint(x, y));
}

Element Document::getElementById(const std::string& id) const
{
    return Element(m_rootElement->getElementById(id));
}

Element Document::documentElement() const
{
    return Element(m_rootElement.get());
}

SVGRootElement* Document::rootElement(bool layoutIfNeeded) const
{
    if(layoutIfNeeded)
        m_rootElement->layoutIfNeeded();
    return m_rootElement.get();
}

Document::Document(Document&&) = default;
Document& Document::operator=(Document&&) = default;

Document::Document() = default;
Document::~Document() = default;

} // namespace lunasvg

extern "C" {

lunasvg_document_t* lunasvg_document_load_from_file(const char* filename) {
    return (lunasvg_document_t*)lunasvg::Document::loadFromFile(filename).release();
}

void lunasvg_document_destroy(lunasvg_document_t* document) {
    delete (lunasvg::Document*)document;
}

lunasvg_bitmap_t* lunasvg_document_render_to_bitmap(lunasvg_document_t* document, uint32_t width, uint32_t height, uint32_t bgColor) {
    auto bitmap = ((lunasvg::Document*)document)->renderToBitmap(width, height, bgColor);
    if(bitmap.isNull()) return nullptr;
    return (lunasvg_bitmap_t*)new lunasvg::Bitmap(std::move(bitmap));
}

void lunasvg_bitmap_destroy(lunasvg_bitmap_t* bitmap) {
    delete (lunasvg::Bitmap*)bitmap;
}

bool lunasvg_bitmap_write_to_png(lunasvg_bitmap_t* bitmap, const char* filename) {
    return ((lunasvg::Bitmap*)bitmap)->writeToPng(filename);
}

}