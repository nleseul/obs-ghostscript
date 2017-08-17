#include <obs-module.h>
#include <util/dstr.h>

#include <stdio.h>
#include <inttypes.h>

// Ghostscript expects a non-standard #define for indicating a Windows environment, apparently.
// This ensures the Ghostscript API functions are referenced by the linker with the correct
// calling convention (on x86). 
#ifdef _WIN32
#define _WINDOWS_
#endif

#include <psi/iapi.h>
#include <devices/gdevdsp.h>

#include <config.h>

static void *shared_ghostscript_instance = NULL;

struct pdf_source {

	char *file_path;
	unsigned int page_number;

	uint32_t width;
	uint32_t height;
	gs_texture_t *texture;

	int cached_rasterwidth;
	int cached_rasterheight;
	int cached_rasterrowsizeinbytes;
	unsigned char *cached_raster;
	bool cached_anypagesrendered;

	obs_source_t *src;
};

static int ghostscript_display_open(void *handle, void *device)
{
	return 0;
}

static int ghostscript_display_preclose(void *handle, void *device)
{
	return 0;
}

static int ghostscript_display_close(void *handle, void *device)
{
	return 0;
}

static int ghostscript_display_presize(void *handle, void *device, int width, int height, int raster, unsigned int format)
{
	return 0;
}

static int ghostscript_display_size(void *handle, void *device, int width, int height, int raster, unsigned int format, unsigned char *pimage)
{
	struct pdf_source *context = handle;

	context->cached_rasterwidth = width;
	context->cached_rasterheight = height;
	context->cached_rasterrowsizeinbytes = raster;
	context->cached_raster = pimage;

	return 0;
}

static int ghostscript_display_sync(void *handle, void *device)
{
	return 0;
}

static int ghostscript_display_page(void *handle, void *device, int copies, int flush)
{
	struct pdf_source *context = handle;

	obs_enter_graphics();

	if (context->texture != NULL)
	{
		gs_texture_destroy(context->texture);
		context->texture = NULL;
	}

	context->cached_anypagesrendered = true;
	context->width = context->cached_rasterwidth;
	context->height = context->cached_rasterheight;
	context->texture = gs_texture_create(context->cached_rasterrowsizeinbytes / 4, context->cached_rasterheight, GS_BGRX, 1, &context->cached_raster, 0);

	obs_leave_graphics();

	return 0;
}

static int ghostscript_display_update(void *handle, void *device,
	int x, int y, int w, int h)
{
	return 0;
}

display_callback display = 
{
	sizeof(display_callback),
	DISPLAY_VERSION_MAJOR,
	DISPLAY_VERSION_MINOR,
	ghostscript_display_open,
	ghostscript_display_preclose,
	ghostscript_display_close,
	ghostscript_display_presize,
	ghostscript_display_size,
	ghostscript_display_sync,
	ghostscript_display_page,
	ghostscript_display_update,
	NULL,
	NULL,
	NULL
};


static void pdf_source_load(struct pdf_source *context)
{
	char display_format_buffer[32] = { 0 };
	char display_handle_buffer[32] = { 0 };
	char page_list_buffer[32] = { 0 };

	snprintf(display_format_buffer, 32, "-dDisplayFormat=%d", DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST |
		DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_TOPFIRST);
	snprintf(display_handle_buffer, 32, "-sDisplayHandle=16#%"PRIx64"", (uint64_t)context);
	snprintf(page_list_buffer, 32, "-sPageList=%d", context->page_number);

	context->cached_anypagesrendered = false;

	if (context->file_path != NULL)
	{
		char *gs_argv[] =
		{
			"obs", // Ignored
			"-sDEVICE=display",
			display_handle_buffer,
			display_format_buffer,
			page_list_buffer,
			"-f",
			context->file_path
		};

		int gs_argc = sizeof(gs_argv) / sizeof(gs_argv[0]);

		// Here, we execute the Ghostscript command to parse the file and render the document. The display device
		// callbacks indicated above will handle copying the Ghostscript buffer into an OBS texture if any page is
		// rendered. 
		gsapi_init_with_args(shared_ghostscript_instance, gs_argc, gs_argv);
		gsapi_exit(shared_ghostscript_instance);
	}

	if (!context->cached_anypagesrendered)
	{
		// If the ghostscript_display_page() callback above was never called, that means the commands issued
		// to Ghostscript did not result in a page in the document being rendered. That is most likely to happen
		// if the document does not contain the requested page number. We simply destroy the texture and render
		// nothing in that case. 
		obs_enter_graphics();
		gs_texture_destroy(context->texture);
		context->texture = NULL;
		obs_leave_graphics();
	}
}


static const char *pdf_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PdfSource");
}

static void pdf_source_update(void *data, obs_data_t *settings)
{
	struct pdf_source *context = data;
	const char *file_path = obs_data_get_string(settings, "file_path");

	if (context->file_path != NULL)
	{
		bfree(context->file_path);
	}
	context->file_path = bstrdup(file_path);
	context->page_number = (unsigned int)obs_data_get_int(settings, "page_number");

	pdf_source_load(context);
}

static void *pdf_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct pdf_source *context = bzalloc(sizeof(struct pdf_source));
	context->src = source;
	
	pdf_source_update(context, settings);

	return context;
}

static void pdf_source_destroy(void *data)
{
	struct pdf_source *context = data;

	if (context->file_path != NULL)
	{
		bfree(context->file_path);
		context->file_path = NULL;
	}

	if (context->texture != NULL)
	{
		gs_texture_destroy(context->texture);
		context->texture = NULL;
	}

	bfree(context);
}

static const char *file_type_filter = "Ghostscript document files (*.pdf *.ps *.eps *.epsf);;";


static obs_properties_t *pdf_source_properties(void *data)
{
	struct pdf_source *context = data;

	obs_properties_t *props = obs_properties_create();
	struct dstr path = { 0 };

	if (context && context->file_path != NULL && context->file_path[0] != 0) 
	{
		const char *slash;

		dstr_copy(&path, context->file_path);
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}

	obs_properties_add_path(props, "file_path",
		obs_module_text("PdfSource.FileName"), OBS_PATH_FILE, file_type_filter, path.array);

	obs_properties_add_int(props, "page_number", obs_module_text("PdfSource.PageNumber"), 1, 9999, 1);

	return props;
}

static void pdf_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct pdf_source *context = data;

	if (context->texture != NULL)
	{
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			context->texture);
		gs_draw_sprite(context->texture, 0,
			context->width, context->height);
	}
}

static void pdf_source_change_page(struct pdf_source *context, 
	bool change_to_previous)
{
	if (change_to_previous && context->page_number > 1)
	{
		context->page_number--;
	}
	else if (!change_to_previous && context->page_number < 9999)
	{
		context->page_number++;
	}

	pdf_source_load(context);
}

static void pdf_source_key_click(void *data,
	const struct obs_key_event *event, bool key_up)
{
	struct pdf_source *context = data;

	if (!key_up)
	{
#ifdef OBS_HAS_NAVKEY_SUPPORT
		switch (event->navigation_keys)
		{
			case NAVKEY_DOWN:
			case NAVKEY_NEXTPAGE:
				pdf_source_change_page(context, false);
				break;

			case NAVKEY_UP:
			case NAVKEY_PREVPAGE:
				pdf_source_change_page(context, true);
				break;
		}
#endif
	}
}

static void pdf_source_mouse_wheel(void *data,
	const struct obs_mouse_event *event, int x_delta, int y_delta)
{
	struct pdf_source *context = data;

	if (y_delta > 0)
	{
		pdf_source_change_page(context, true);
	}
	else if (y_delta < 0)
	{
		pdf_source_change_page(context, false);
	}
}

static uint32_t pdf_source_getwidth(void *data)
{
	struct pdf_source *context = data;
	return context->width;
}

static uint32_t pdf_source_getheight(void *data)
{
	struct pdf_source *context = data;
	return context->height;
}

static void pdf_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "page_number", 1);
}

struct obs_source_info pdf_source_info = {
	.id             = "obs_pdf",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	.create         = pdf_source_create,
	.destroy        = pdf_source_destroy,
	.update         = pdf_source_update,
	.get_name       = pdf_source_get_name,
	.get_defaults   = pdf_source_defaults,
	.get_width      = pdf_source_getwidth,
	.get_height     = pdf_source_getheight,
	.video_render   = pdf_source_render,
	.get_properties = pdf_source_properties,
	.key_click		= pdf_source_key_click,
	.mouse_wheel	= pdf_source_mouse_wheel
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-pdf", "en-US")

bool obs_module_load(void)
{
	gsapi_new_instance(&shared_ghostscript_instance, NULL);
	gsapi_set_display_callback(shared_ghostscript_instance, &display);

	obs_register_source(&pdf_source_info);

	return true;
}

void obs_module_unload(void)
{
	gsapi_delete_instance(shared_ghostscript_instance);
	shared_ghostscript_instance = NULL;
}
