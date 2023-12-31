// The Metal version is still an untested work-in-progress
#if defined(HELLOIMGUI_HAS_METAL)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "hello_imgui/internal/backend_impls/rendering_metal.h"
#include "renderpass.h"
#include "shader.h"

RenderPass::RenderPass(bool write_depth, bool clear) :
    m_clear(clear), m_clear_color(0, 0, 0, 0), m_clear_depth(1.f), m_viewport_offset(0), m_viewport_size(0),
    m_framebuffer_size(0), m_depth_test(write_depth ? DepthTest::Less : DepthTest::Always), m_depth_write(write_depth),
    m_cull_mode(CullMode::Back), m_active(false), m_command_buffer(nullptr), m_command_encoder(nullptr)
{
    m_pass_descriptor = [MTLRenderPassDescriptor new];

    set_clear_color(m_clear_color);
    set_clear_depth(m_clear_depth);
}

RenderPass::~RenderPass()
{
    MTLRenderPassDescriptor *pass_descriptor = (MTLRenderPassDescriptor *)m_pass_descriptor;
    [pass_descriptor release];
}

void RenderPass::begin()
{
#if !defined(NDEBUG)
    if (m_active)
        throw std::runtime_error("RenderPass::begin(): render pass is already active!");
#endif

    auto &gMetalGlobals = HelloImGui::GetMetalGlobals();

    id<MTLCommandBuffer> command_buffer = [gMetalGlobals.mtlCommandQueue commandBuffer];

    MTLRenderPassDescriptor *pass_descriptor = (__bridge MTLRenderPassDescriptor *)m_pass_descriptor;

    bool clear_manual = m_clear && (m_viewport_offset != int2(0, 0) || m_viewport_size != m_framebuffer_size);

    pass_descriptor.colorAttachments[0].texture     = gMetalGlobals.caMetalDrawable.texture;
    pass_descriptor.colorAttachments[0].loadAction  = m_clear && !clear_manual ? MTLLoadActionClear : MTLLoadActionLoad;
    pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> command_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_descriptor];

    [command_encoder setFrontFacingWinding:MTLWindingCounterClockwise];

    m_command_buffer  = (__bridge_retained void *)command_buffer;
    m_command_encoder = (__bridge_retained void *)command_encoder;
    m_active          = true;

    set_viewport(m_viewport_offset, m_viewport_size);

    // if (clear_manual)
    // {
    //     MTLDepthStencilDescriptor *depth_desc = [MTLDepthStencilDescriptor new];
    //     depth_desc.depthCompareFunction       = MTLCompareFunctionAlways;
    //     depth_desc.depthWriteEnabled          = m_targets[0].get() != nullptr;
    //     id<MTLDevice>            device       = (__bridge id<MTLDevice>)metal_device();
    //     id<MTLDepthStencilState> depth_state  = [device newDepthStencilStateWithDescriptor:depth_desc];
    //     [command_encoder setDepthStencilState:depth_state];

    //     if (!m_clear_shader)
    //     {
    //         m_clear_shader = new Shader(this,

    //                                     "clear_shader",

    //                                     /* Vertex shader */
    //                                     R"(using namespace metal;

    //             struct VertexOut {
    //                 float4 position [[position]];
    //             };

    //             vertex VertexOut vertex_main(const device float2 *position,
    //                                          constant float &clear_depth,
    //                                          uint id [[vertex_id]]) {
    //                 VertexOut vert;
    //                 vert.position = float4(position[id], clear_depth, 1.f);
    //                 return vert;
    //             })",

    //                                     /* Fragment shader */
    //                                     R"(using namespace metal;

    //             struct VertexOut {
    //                 float4 position [[position]];
    //             };

    //             fragment float4 fragment_main(VertexOut vert [[stage_in]],
    //                                           constant float4 &clear_color) {
    //                 return clear_color;
    //             })");

    //         const float positions[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f};

    //         m_clear_shader->set_buffer("position", VariableType::Float32, {6, 2}, positions);
    //     }

    //     m_clear_shader->set_uniform("clear_color", m_clear_color.at(0));
    //     m_clear_shader->set_uniform("clear_depth", m_clear_depth);
    //     m_clear_shader->begin();
    //     m_clear_shader->draw_array(Shader::PrimitiveType::Triangle, 0, 6, false);
    //     m_clear_shader->end();
    // }

    set_depth_test(m_depth_test, m_depth_write);
    set_cull_mode(m_cull_mode);
}

void RenderPass::end()
{
#if !defined(NDEBUG)
    if (!m_active)
        throw std::runtime_error("RenderPass::end(): render pass is not active!");
#endif
    id<MTLCommandBuffer>        command_buffer  = (__bridge_transfer id<MTLCommandBuffer>)m_command_buffer;
    id<MTLRenderCommandEncoder> command_encoder = (__bridge_transfer id<MTLRenderCommandEncoder>)m_command_encoder;
    [command_encoder endEncoding];
    [command_buffer commit];
    m_command_encoder = nullptr;
    m_command_buffer  = nullptr;
    m_active          = false;
}

void RenderPass::resize(const int2 &size)
{
    m_framebuffer_size = size;
    m_viewport_offset  = int2(0, 0);
    m_viewport_size    = size;
}

void RenderPass::set_clear_color(const float4 &color)
{
    m_clear_color = color;

    MTLRenderPassDescriptor *pass_descriptor = (__bridge MTLRenderPassDescriptor *)m_pass_descriptor;

    pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(color.x, color.y, color.z, color.w);
}

void RenderPass::set_clear_depth(float depth)
{
    m_clear_depth = depth;

    MTLRenderPassDescriptor *pass_descriptor   = (__bridge MTLRenderPassDescriptor *)m_pass_descriptor;
    pass_descriptor.depthAttachment.clearDepth = depth;
}

void RenderPass::set_viewport(const int2 &offset, const int2 &size)
{
    m_viewport_offset = offset;
    m_viewport_size   = size;
    if (m_active)
    {
        id<MTLRenderCommandEncoder> command_encoder = (__bridge id<MTLRenderCommandEncoder>)m_command_encoder;
        [command_encoder
            setViewport:(MTLViewport){(double)offset.x, (double)offset.y, (double)size.x, (double)size.y, 0.0, 1.0}];
        int2 scissor_size   = max(min(offset + size, m_framebuffer_size) - offset, int2(0));
        int2 scissor_offset = max(min(offset, m_framebuffer_size), int2(0));
        [command_encoder setScissorRect:(MTLScissorRect){(NSUInteger)scissor_offset.x, (NSUInteger)scissor_offset.y,
                                                         (NSUInteger)scissor_size.x, (NSUInteger)scissor_size.y}];
    }
}

void RenderPass::set_depth_test(DepthTest depth_test, bool depth_write)
{
    m_depth_test  = depth_test;
    m_depth_write = depth_write;
    if (m_active)
    {
        MTLDepthStencilDescriptor *depth_desc = [MTLDepthStencilDescriptor new];

        MTLCompareFunction func;
        switch (depth_test)
        {
        case DepthTest::Never: func = MTLCompareFunctionNever; break;
        case DepthTest::Less: func = MTLCompareFunctionLess; break;
        case DepthTest::Equal: func = MTLCompareFunctionEqual; break;
        case DepthTest::LessEqual: func = MTLCompareFunctionLessEqual; break;
        case DepthTest::Greater: func = MTLCompareFunctionGreater; break;
        case DepthTest::NotEqual: func = MTLCompareFunctionNotEqual; break;
        case DepthTest::GreaterEqual: func = MTLCompareFunctionGreater; break;
        case DepthTest::Always: func = MTLCompareFunctionAlways; break;
        default: throw std::runtime_error("Shader::set_depth_test(): invalid depth test mode!");
        }
        depth_desc.depthCompareFunction = func;
        depth_desc.depthWriteEnabled    = depth_write;

        auto &gMetalGlobals = HelloImGui::GetMetalGlobals();

        id<MTLDepthStencilState> depth_state =
            [gMetalGlobals.caMetalLayer.device newDepthStencilStateWithDescriptor:depth_desc];
        id<MTLRenderCommandEncoder> command_encoder = (__bridge id<MTLRenderCommandEncoder>)m_command_encoder;
        [command_encoder setDepthStencilState:depth_state];
    }
}

void RenderPass::set_cull_mode(CullMode cull_mode)
{
    m_cull_mode = cull_mode;
    if (m_active)
    {
        MTLCullMode cull_mode_mtl;
        switch (cull_mode)
        {
        case CullMode::Front: cull_mode_mtl = MTLCullModeFront; break;
        case CullMode::Back: cull_mode_mtl = MTLCullModeBack; break;
        case CullMode::Disabled: cull_mode_mtl = MTLCullModeNone; break;
        default: throw std::runtime_error("Shader::set_cull_mode(): invalid cull mode!");
        }
        id<MTLRenderCommandEncoder> command_encoder = (__bridge id<MTLRenderCommandEncoder>)m_command_encoder;
        [command_encoder setCullMode:cull_mode_mtl];
    }
}

#endif // defined(HELLOIMGUI_HAS_METAL)