#include "shader.h"

using std::string;

void Shader::set_buffer_divisor(const std::string &name, size_t divisor)
{
    auto it = m_buffers.find(name);
    if (it == m_buffers.end())
        throw std::runtime_error("Shader::set_buffer_divisor(): could not find argument named \"" + name + "\"");

    Buffer &buf          = m_buffers[name];
    buf.instance_divisor = divisor;
    buf.dirty            = true;
}

void Shader::set_buffer_pointer_offset(const std::string &name, size_t offset)
{
    auto it = m_buffers.find(name);
    if (it == m_buffers.end())
        throw std::runtime_error("Shader::set_buffer_pointer_offset(): could not find argument named \"" + name + "\"");

    Buffer &buf        = m_buffers[name];
    buf.pointer_offset = offset;
    buf.dirty          = true;
}

std::string Shader::Buffer::to_string() const
{
    std::string result = "Buffer[type=";
    switch (type)
    {
    case BufferType::VertexBuffer: result += "vertex"; break;
    case BufferType::FragmentBuffer: result += "fragment"; break;
    case BufferType::UniformBuffer: result += "uniform"; break;
    case BufferType::IndexBuffer: result += "index"; break;
    default: result += "unknown"; break;
    }
    result += ", dtype=";
    result += type_name(dtype);
    result += ", shape=[";
    for (size_t i = 0; i < ndim; ++i)
    {
        result += std::to_string(shape[i]);
        if (i + 1 < ndim)
            result += ", ";
    }
    result += "]]";
    return result;
}
