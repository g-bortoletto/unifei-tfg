// --------------------------------------------------------------------------------------------------------------------

#include "types.h"

#include "image.h"

#include "point.h"

#include "triangle.h"

#include "gui.h"

// --------------------------------------------------------------------------------------------------------------------

#define IMGUI_IMPLEMENTATION

#include "../include/imgui/imgui_single_file.h"

// --------------------------------------------------------------------------------------------------------------------

#define SOKOL_IMPL

#define SOKOL_GLCORE33

#include "../include/sokol/sokol_app.h"

#include "../include/sokol/sokol_gfx.h"

#include "../include/sokol/sokol_gp.h"

#include "../include/sokol/sokol_log.h"

#include "../include/sokol/sokol_glue.h"

#include "../include/fonts/segoeui.h"

#include "../include/sokol/sokol_imgui.h"

// --------------------------------------------------------------------------------------------------------------------

#include "../include/nfd/nfd_win.cpp"

// --------------------------------------------------------------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION

#include "../include/stb/stb_image.h"

// --------------------------------------------------------------------------------------------------------------------

#include <string.h>

#include <vector>

using namespace std;

// --------------------------------------------------------------------------------------------------------------------

#define global static

// --------------------------------------------------------------------------------------------------------------------

global program_state_t program;

// --------------------------------------------------------------------------------------------------------------------

static void frame(void) 
{
	// update window size
	program.window_width = sapp_width();
	program.window_height = sapp_height();
	program.window_ratio = program.window_width / (float)program.window_height;

	// update viewport size
	program.viewport_x = program.side_bar_width;
	program.viewport_y = program.main_menu_bar_height;
	program.viewport_width = program.window_width - program.side_bar_width;
	program.viewport_height = program.window_height - program.main_menu_bar_height;
	program.viewport_ratio = program.viewport_width / (float)program.viewport_height;

	sgp_begin(program.window_width, program.window_height);
	simgui_new_frame({ program.window_width, program.window_height, sapp_frame_duration(), sapp_dpi_scale() });

	sgp_set_color(0.05f, 0.05f, 0.05f, 1.0f);
	sgp_clear();
	sgp_reset_color();

	draw_main_menu_bar(&program);
	draw_side_bar(&program);
	draw_main_image(&program);
	draw_triangles(&program);

	sg_pass_action pass_action = { };
	sg_begin_default_pass(&pass_action, program.window_width, program.window_height);
	sgp_reset_pipeline();
	sgp_flush();
	sgp_end();
	simgui_render();
	sg_end_pass();
	sg_commit();
}

// --------------------------------------------------------------------------------------------------------------------

static void init(void) 
{
	// setup sokol-gfx and sokol-time
	sg_desc desc = { };
	desc.context = sapp_sgcontext();
	desc.logger.func = slog_func;
	sg_setup(&desc);

	simgui_desc_t simgui_desc = { };
	simgui_desc.no_default_font = true;
	simgui_setup(&simgui_desc);

	auto& io = ImGui::GetIO();
	ImFontConfig fontCfg;
	fontCfg.FontDataOwnedByAtlas = false;
	fontCfg.OversampleH = 2;
	fontCfg.OversampleV = 2;
	fontCfg.RasterizerMultiply = 1.5f;
	io.Fonts->AddFontFromMemoryCompressedTTF(
		segoeui_compressed_data, 
		segoeui_compressed_size, 
		16.0f, 
		&fontCfg);

	unsigned char* font_pixels;
	int font_width, font_height;
	io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
	sg_image_desc img_desc = { };
	img_desc.width = font_width;
	img_desc.height = font_height;
	img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
	img_desc.wrap_u = SG_WRAP_CLAMP_TO_BORDER;
	img_desc.wrap_v = SG_WRAP_CLAMP_TO_BORDER;
	img_desc.min_filter = SG_FILTER_LINEAR;
	img_desc.mag_filter = SG_FILTER_LINEAR;
	img_desc.data.subimage[0][0].ptr = font_pixels;
	img_desc.data.subimage[0][0].size = font_width * font_height * 4;
	io.Fonts->TexID = (ImTextureID)(uintptr_t) sg_make_image(&img_desc).id;

	sgp_desc sgpdesc = { 0 };
	sgp_setup(&sgpdesc);
	if(!sgp_is_valid()) 
	{
		fprintf(stderr, "Failed to create Sokol GP context: %s\n", sgp_get_error_message(sgp_get_last_error()));
		exit(-1);
	}
}

// --------------------------------------------------------------------------------------------------------------------

static void cleanup(void) 
{
	sg_destroy_image(program.main_image);
	simgui_shutdown();
	sgp_shutdown();
	sg_shutdown();
}

// --------------------------------------------------------------------------------------------------------------------

static void input(const sapp_event* e) 
{
	simgui_handle_event(e);

	program.is_mouse_in_viewport = e->mouse_x > program.viewport_x && e->mouse_y > program.viewport_y;
	program.mouse_delta = { e->mouse_dx, e->mouse_dy };
	program.mouse_position = { e->mouse_x, e->mouse_y };

	if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN && 
		e->mouse_button == SAPP_MOUSEBUTTON_LEFT)
	{
		program.is_mouse_left_down = true;
		if (program.is_mouse_in_viewport)
		{

			program.last_selected = program.selected;
			program.selected = 0;
			for (auto shape : program.shape_list)
			{
				if (point_is_inside_triangle(program.mouse_position, shape.second))
				{
					program.selected = shape.first;
				}
			}
		}
	}

	if (e->type == SAPP_EVENTTYPE_MOUSE_UP &&
		e->mouse_button ==SAPP_MOUSEBUTTON_LEFT)
	{
		program.is_mouse_left_down = false;
	}

	if (program.is_mouse_in_viewport &&
		program.selected && 
		program.is_mouse_left_down)
	{
		bool mouse_close_p0 = point_distance(
			program.shape_list[program.selected].p[0], 
			program.mouse_position) <= 20.0f;
		bool mouse_close_p1 = point_distance(
			program.shape_list[program.selected].p[1], 
			program.mouse_position) <= 20.0f;
		bool mouse_close_p2 = point_distance(
			program.shape_list[program.selected].p[2], 
			program.mouse_position) <= 20.0f;
		if (program.last_selected == program.selected)
		{
			if (mouse_close_p0)
			{
				point_move(&program.shape_list[program.selected], 0, { e->mouse_dx, e->mouse_dy });
			}
			else if (mouse_close_p1)
			{
				point_move(&program.shape_list[program.selected], 1, { e->mouse_dx, e->mouse_dy });
			}
			else if (mouse_close_p2)
			{
				point_move(&program.shape_list[program.selected], 2, { e->mouse_dx, e->mouse_dy });
			}
		}
		else
		{
			program.shape_list[program.selected].p[0].x += e->mouse_dx;
			program.shape_list[program.selected].p[0].y += e->mouse_dy;
			
			program.shape_list[program.selected].p[1].x += e->mouse_dx;
			program.shape_list[program.selected].p[1].y += e->mouse_dy;
			
			program.shape_list[program.selected].p[2].x += e->mouse_dx;
			program.shape_list[program.selected].p[2].y += e->mouse_dy;
		}
	}

	sapp_set_window_title(program.is_mouse_left_down ? "down" : "up");
}

// --------------------------------------------------------------------------------------------------------------------

sapp_desc sokol_main(int argc, char* argv[]) 
{
	(void)argc;
	(void)argv;
	sapp_desc desc = { };
	desc.init_cb = init;
	desc.frame_cb = frame;
	desc.cleanup_cb = cleanup;
	desc.event_cb = input;
	desc.width = 1280;
	desc.height = 720;
	desc.high_dpi = true;
	desc.ios_keyboard_resizes_canvas = false;
	desc.gl_force_gles2 = true;
	desc.window_title = "tfg";
	desc.icon.sokol_default = true;
	desc.enable_clipboard = true;
	desc.logger.func = slog_func;
	return desc;
}