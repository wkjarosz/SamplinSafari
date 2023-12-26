/**
    \file shader.h
*/
#pragma once

#include "linalg.h"
#include "traits.h"
#include <string>
#include <unordered_map>

/// An abstraction for shaders that work with OpenGL, OpenGL ES, and (at some point down the road, hopefully) Metal.
/*
    This is adapted from NanoGUI's Shader class. Copyright follows.
    ----------
    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/
class Shader
{
public:
    /// The type of geometry that should be rendered
    enum class PrimitiveType
    {
        Point,
        Line,
        LineStrip,
        LineLoop,
        Triangle,
        TriangleStrip,
        TriangleFan
    };

    /// Alpha blending mode
    enum class BlendMode
    {
        None,
        AlphaBlend // alpha * new_color + (1 - alpha) * old_color
    };

    /**
        Initialize the shader using the source files (read from the assets directory).

        \param name
            A name identifying this shader

        \param vs_filename
            Filename of the vertex shader source code.

        \param fs_filename
            Filename of the fragment shader source code.
    */
    Shader(const std::string &name, const std::string &vs_filename, const std::string &fs_filename,
           BlendMode blend_mode = BlendMode::None);

    /// Return the render pass associated with this shader
    // RenderPass *render_pass() { return m_render_pass; }

    /// Return the name of this shader
    const std::string &name() const
    {
        return m_name;
    }

    /// Return the blending mode of this shader
    BlendMode blend_mode() const
    {
        return m_blend_mode;
    }

    /**
        Upload a buffer (e.g. vertex positions) that will be associated with a named shader parameter.

        Note that this function should be used both for 'varying' and 'uniform'
        data---the implementation takes care of routing the data to the right
        endpoint. Matrices should be specified in column-major order.

        The buffer will be replaced if it is already present.
     */
    void set_buffer(const std::string &name, VariableType type, size_t ndim, const size_t *shape, const void *data);

    void set_buffer(const std::string &name, VariableType type, std::initializer_list<size_t> shape, const void *data)
    {
        set_buffer(name, type, shape.end() - shape.begin(), shape.begin(), data);
    }

    // std::vectors
    template <typename T, int M, int N>
    void set_buffer(const std::string &name, const std::vector<linalg::mat<T, M, N>> &mats)
    {
        size_t shape[3] = {mats.size(), M, N};
        set_buffer(name, get_type<T>(), 3, shape, mats.data());
    }

    template <typename T, int M>
    void set_buffer(const std::string &name, const std::vector<linalg::vec<T, M>> &vecs)
    {
        size_t shape[3] = {vecs.size(), M, 1};
        set_buffer(name, get_type<T>(), 2, shape, vecs.data());
    }

    template <typename T>
    void set_buffer(const std::string &name, const std::vector<T> &data)
    {
        size_t shape[3] = {data.size(), 1, 1};
        set_buffer(name, get_type<T>(), 1, shape, data.data());
    }

    // set_uniform
    template <typename T, int M>
    void set_uniform(const std::string &name, const linalg::vec<T, M> &value)
    {
        size_t shape[3] = {M, 1, 1};
        set_buffer(name, get_type<T>(), 1, shape, &value);
    }

    template <typename T, int M, int N>
    void set_uniform(const std::string &name, const linalg::mat<T, M, N> &value)
    {
        size_t shape[3] = {M, N, 1};
        set_buffer(name, get_type<T>(), 2, shape, &value);
    }

    /// Upload a uniform variable (e.g. a vector or matrix) that will be associated with a named shader parameter.
    template <typename T>
    void set_uniform(const std::string &name, const T &value)
    {
        size_t shape[3] = {1, 1, 1};
        if constexpr (std::is_scalar_v<T>)
            set_buffer(name, get_type<T>(), 0, shape, &value);
        else
            throw std::runtime_error("Shader::set_uniform(): invalid input array dimension!");
    }

    /**
        Set the "rate at which generic vertex attributes advance when rendering multiple instances"
        see:
            https://registry.khronos.org/OpenGL-Refpages/es3/html/glVertexAttribDivisor.xhtml
            https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribDivisor.xhtml
    */
    void set_buffer_divisor(const std::string &name, size_t divisor);

    // /**
    //  * \brief Associate a texture with a named shader parameter
    //  *
    //  * The association will be replaced if it is already present.
    //  */
    // void set_texture(const std::string &name, Texture *texture);

    /**
        Begin drawing using this shader

        Note that any updates to 'uniform' and 'varying' shader parameters
        *must* occur prior to this method call.

        The Python bindings also include extra \c __enter__ and \c __exit__
        aliases so that the shader can be activated via Pythons 'with'
        statement.
    */
    void begin();

    /// End drawing using this shader
    void end();

    /**
        Render geometry arrays, either directly or using an index array.

        \param primitive_type
            What type of geometry should be rendered?

        \param offset
            First index to render. Must be a multiple of 2 or 3 for lines and triangles, respectively (unless specified
            using strips).

        \param offset
            Number of indices to render. Must be a multiple of 2 or 3 for lines and triangles, respectively (unless
            specified using strips).

        \param indexed
            Render indexed geometry? In this case, an \c uint32_t valued buffer with name \c indices must have been
            uploaded using \ref set().

        \param instances
            If you want to render instances, set this to > 0. In this case, make sure to call \ref set_buffer_divisor()
       for each registered buffer beforehand to inform the shader how the instances should access the buffers.
    */
    void draw_array(PrimitiveType primitive_type, size_t offset, size_t count, bool indexed = false,
                    size_t instances = 0u);

#if defined(HELLOIMGUI_HAS_OPENGL)
    uint32_t shader_handle() const
    {
        return m_shader_handle;
    }
#elif defined(HELLOIMGUI_USE_METAL)
    void *pipeline_state() const
    {
        return m_pipeline_state;
    }
#endif

#if defined(HELLOIMGUI_USE_GLAD)
    uint32_t vertex_array_handle() const
    {
        return m_vertex_array_handle;
    }
#endif

protected:
    enum BufferType
    {
        Unknown = 0,
        VertexBuffer,
        VertexTexture,
        VertexSampler,
        FragmentBuffer,
        FragmentTexture,
        FragmentSampler,
        UniformBuffer,
        IndexBuffer,
    };

    struct Buffer
    {
        void        *buffer = nullptr;
        BufferType   type   = Unknown;
        VariableType dtype  = VariableType::Invalid;
        int          index  = 0;
        size_t       ndim   = 0;
        size_t       shape[3]{0, 0, 0};
        size_t       size             = 0;
        size_t       instance_divisor = 0;
        bool         dirty            = false;

        std::string to_string() const;
    };

    /// Release all resources
    virtual ~Shader();

protected:
    // RenderPass* m_render_pass;
    std::string                             m_name;
    std::unordered_map<std::string, Buffer> m_buffers;
    BlendMode                               m_blend_mode;

#if defined(HELLOIMGUI_HAS_OPENGL)
    uint32_t m_shader_handle = 0;
#if defined(HELLOIMGUI_USE_GLAD)
    uint32_t m_vertex_array_handle = 0;
    bool     m_uses_point_size     = false;
#endif
#elif defined(HELLOIMGUI_USE_METAL)
    void *m_pipeline_state;
#endif
};

bool check_glerror(const char *cmd);

#define CHK(cmd)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        cmd;                                                                                                           \
        while (check_glerror(#cmd))                                                                                    \
        {                                                                                                              \
        }                                                                                                              \
    } while (false)
