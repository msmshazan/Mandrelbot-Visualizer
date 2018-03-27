/*
* Copyright 2011-2018 Branimir Karadzic. All rights reserved.
* License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
*/

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "logo.h"
#include <stdio.h>
#include <math.h>

#include <bx/string.h>
#include <bx/timer.h>
#include <bimg/decode.h>

#include "entry/entry.h"
#include "imgui/imgui.h"
#include "nanovg/nanovg.h"

BX_PRAGMA_DIAGNOSTIC_PUSH();
BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter");
#define BLENDISH_IMPLEMENTATION
#include "blendish.h"
BX_PRAGMA_DIAGNOSTIC_POP();
namespace
{
	typedef struct {
		void * Data;
		int Width;
		int Height;
		int Pitch;
		int Size;
	}BackBuffer;

	struct PosTexCoord0Vertex
	{
		float x;
		float y;
		float z;
		float u;
		float v; 
		static void init()
		{
			Declaration
				.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float, true)
				.end();
		};

		static bgfx::VertexDecl Declaration;
	};

	bgfx::VertexDecl PosTexCoord0Vertex::Declaration;

	static PosTexCoord0Vertex CubeVertices[] =
	{
		{ -1.0f,  -1.0f,  0.0f, 0 ,0 },
		{ 1.0f,  -1.0f,  0.0f, 1,0 },
		{ -1.0f, 1.0f,  0.0f, 0,1 },
		{ 1.0f, 1.0f,  0.0f, 1,1 },
	};


	static const uint16_t QuadTriStrip[] =
	{
		0, 1, 2,3
	};


	BackBuffer CreateBackBuffer(int w, int h) {
		BackBuffer Result = {};
		Result.Width = w;
		Result.Height = h;
		Result.Pitch = w * sizeof(uint32_t);
		Result.Size = w*h * sizeof(uint32_t);
		Result.Data = malloc(h*w * sizeof(uint32_t));
		return Result;
	}

	class ExampleHelloWorld : public entry::AppI
	{

		BackBuffer backBuffer;
		bgfx::TextureHandle Buffer;
		bgfx::UniformHandle UniformTexColor;
		bgfx::UniformHandle UniformCenter;
		bgfx::UniformHandle UniformScale;
		bgfx::UniformHandle UniformIterations;
		float Center[2] = { 0,0 };
		int Iterations = 100;
		float Scale = 6.f;


		entry::MouseState AppMouseState;

		int64_t TimeOffset;
		entry::WindowState Windowstate;
		NVGcontext* nvg;

		bgfx::VertexBufferHandle VertexBuffer;
		bgfx::IndexBufferHandle IndexBuffer;
		bgfx::ProgramHandle ShaderProgram;
		uint32_t Width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;
	public:
		ExampleHelloWorld(const char* _name, const char* _description)
			: entry::AppI(_name, _description)
		{
		}

		void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
		{
			Args args(_argc, _argv);

			Width = _width;
			m_height = _height;
			m_debug = BGFX_DEBUG_TEXT |   BGFX_DEBUG_PROFILER;
			m_reset = BGFX_RESET_NONE;

			bgfx::init(args.m_type, args.m_pciId);
			bgfx::reset(Width, m_height, m_reset);
			
			// Enable debug text.
			bgfx::setDebug(m_debug);
			backBuffer = CreateBackBuffer(1920,1080);
			for (size_t i = 0; i < backBuffer.Size; i++)
			{
				((char *)backBuffer.Data)[i] = 0;
			}
			Buffer = loadTexture("textures/gradient.png");
			UniformTexColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Int1);
			UniformIterations = bgfx::createUniform("s_iter", bgfx::UniformType::Vec4);
			UniformCenter = bgfx::createUniform("s_center", bgfx::UniformType::Vec4);
			UniformScale = bgfx::createUniform("s_scale", bgfx::UniformType::Vec4);
			// Set view 0 clear state.
			bgfx::setViewClear(0
				, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
				, 0x303030ff
				, 1.0f
				, 0
			);
			imguiCreate();
			nvg = nvgCreate(1, 0);
			bgfx::setViewMode(0, bgfx::ViewMode::Sequential);
			// Create vertex stream declaration.
			PosTexCoord0Vertex::init();

			// Create static vertex buffer.
			VertexBuffer = bgfx::createVertexBuffer(
				// Static data can be passed with bgfx::makeRef
				bgfx::makeRef(CubeVertices, sizeof(CubeVertices))
				, PosTexCoord0Vertex::Declaration
			);

			// Create static index buffer.
			IndexBuffer = bgfx::createIndexBuffer(
				// Static data can be passed with bgfx::makeRef
				bgfx::makeRef(QuadTriStrip, sizeof(QuadTriStrip))
			);

			// Create program from shaders.
			ShaderProgram = loadProgram("vs_demo", "fs_demo");

			TimeOffset = bx::getHPCounter();
			
		}

		virtual int shutdown() override
		{
			imguiDestroy();
			bgfx::destroy(IndexBuffer);
			bgfx::destroy(VertexBuffer);
			bgfx::destroy(UniformTexColor);
			bgfx::destroy(UniformCenter);
			bgfx::destroy(UniformIterations);
			bgfx::destroy(UniformScale);
			nvgDelete(nvg);
			bgfx::destroy(ShaderProgram);
			// Shutdown bgfx.

			bgfx::destroy(Buffer);
			bgfx::shutdown();
			return 0;
		}

		
		void putpixel(int x, int y,float r,float g,float b , float a, BackBuffer* buffer) {
			
			if (x < buffer->Width && y < buffer->Height) {
				char *pixel = (char *)buffer->Data;
				pixel += buffer->Pitch*y;
				pixel += (x * sizeof(uint32_t));
				uint32_t R = ((uint32_t)r * 255 << 16) & 0x00ff0000;
				uint32_t G = ((uint32_t)g * 255 << 8) & 0x0000ff00;
				uint32_t B = ((uint32_t)b * 255 << 0) & 0x000000ff;
				uint32_t A = ((uint32_t)a * 255 << 24) & 0xff000000;
				uint32_t color = R | G | B | A;
				*(uint32_t *)pixel = color;
			}

		}

		void Cantor(int x, int y, int len ,int displacement, BackBuffer * buffer) {

			x = (x % buffer->Width);
			y = (y % buffer->Height);

				if (len >= 1) {
					
					for (size_t i = x; i < x + len; i++)
					{
						putpixel(i, y,0,0,1,1,buffer );
					}
					y += displacement;
					Cantor(x, y, len / 3,displacement, buffer);
					Cantor(x + len * 2 / 3, y, len  / 3,displacement, buffer);
				}
			
		}

		void MandrelbotSet(int iterations,BackBuffer *buffer) {
			double scale = 5.0;
			float strength = 0.f;
			int max = iterations;
			for (int row = 0; row < buffer->Height; row++) {
				for (int col = 0; col < buffer->Width; col++) {
					double c_re = (col - buffer->Width / 2.0)*scale / buffer->Width;
					double c_im = (row - buffer->Height / 2.0)*scale / buffer->Width;
					double x = 0, y = 0;
					int iteration = 0;
					while (x*x + y*y <= scale && iteration < max) {
						double x_new = x*x - y*y + c_re;
						y = 2 * x*y + c_im;
						x = x_new;
						iteration++;
						strength += 1.f / max;
					}
					if (iteration < max) {
						putpixel(col, row, 1,0,1,1,buffer);
					}
					else {
						putpixel(col, row, 0,1,0,1,buffer);
					}
				}
			}
		}
		bool update() override
		{
			if (!entry::processWindowEvents(Windowstate, m_debug, m_reset ))
			{
				
				AppMouseState = Windowstate.m_mouse;
				{
					imguiBeginFrame(AppMouseState.m_mx
						, AppMouseState.m_my
						, (AppMouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
						| (AppMouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
						| (AppMouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
						, AppMouseState.m_mz
						, uint16_t(Width)
						, uint16_t(m_height)
					);
					showExampleDialog(this);
					ImGui::Begin("Window");
					ImGui::SliderInt("Iterator",&Iterations, 1, 200);
					ImGui::DragFloat("Zoom", &Scale,0.1, 0, 50);
					ImGui::DragFloat("Centre Pos X:", &Center[0], 0.01, -2, 2);
					ImGui::DragFloat("Centre Pos Y:", &Center[1], 0.01, -2, 2);
					ImGui::End();
					imguiEndFrame();

				}
					//memset(backBuffer.Data, 0, backBuffer.Size);
					int64_t now = bx::getHPCounter();
					const double freq = double(bx::getHPFrequency());
					float time = (float)((now - TimeOffset) / freq);
					// Set view 0 default viewport.
					bgfx::setViewRect(0, 0, 0, uint16_t(Width), uint16_t(m_height));
					bgfx::touch(0);
					nvgBeginFrame(nvg, Width, m_height, 1.0f);
					nvgEndFrame(nvg);
					bgfx::setViewRect(1, 0, 0, uint16_t(Width), uint16_t(m_height));
					bgfx::touch(1);
					time = (float)((bx::getHPCounter() - TimeOffset) / double(bx::getHPFrequency()));
					// Submit 11x11 cubes.
						{
						float mtx[16] = {};
							bx::mtxIdentity(mtx);
							// Set model matrix for rendering.
							bgfx::setUniform(UniformCenter,Center);
							float iterator = (float)Iterations;
							bgfx::setUniform(UniformIterations, &iterator); 
							bgfx::setUniform(UniformScale, &Scale); 
							//bgfx::setTransform(mtx);
							// Set vertex and index buffer.
							bgfx::setVertexBuffer(0, VertexBuffer);
							bgfx::setIndexBuffer(IndexBuffer);
							// Bind texture
						    bgfx::setTexture(0, UniformTexColor, Buffer);
							// Set render state
							bgfx::setState(0
								| (BGFX_STATE_WRITE_R )
								| (BGFX_STATE_WRITE_G )
								| (BGFX_STATE_WRITE_B )
								| (BGFX_STATE_WRITE_A )
								| BGFX_STATE_WRITE_Z
								| BGFX_STATE_DEPTH_TEST_LESS
								| BGFX_STATE_CULL_CW
								| BGFX_STATE_MSAA
								| BGFX_STATE_PT_TRISTRIP );

							// Submit primitive for rendering to view 0.
							bgfx::submit(0, ShaderProgram);
							
						}
					
					// Use debug font to print information about this example.
					bgfx::dbgTextClear();
					// Advance to next frame. Rendering thread will be kicked to
					// process submitted rendering primitives.

					bgfx::frame();
					
				return true;
			}

			return false;
		}
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(ExampleHelloWorld, "BGFX_APP", "Test APP");
