#include <obs-module.h>
#include <obs-hotkey.h>
#include <util/darray.h>
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

	bool should_override_page_size;
	int override_width;
	int override_height;
	bool override_fit_to_page;

	bool should_override_dpi;
	int override_dpi;

	int cached_rasterwidth;
	int cached_rasterheight;
	int cached_rasterrowsizeinbytes;
	unsigned char *cached_raster;
	bool cached_anypagesrendered;

	obs_hotkey_pair_id change_page_hotkey_pair;

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

	if (context->texture != NULL && (context->width != context->cached_rasterwidth || context->height != context->cached_rasterheight))
	{
		gs_texture_destroy(context->texture);
		context->texture = NULL;
	}

	context->cached_anypagesrendered = true;
	context->width = context->cached_rasterwidth;
	context->height = context->cached_rasterheight;

	if (context->texture == NULL)
	{
		context->texture = gs_texture_create(context->cached_rasterrowsizeinbytes / 4, context->cached_rasterheight, 
			GS_BGRX, 1, &context->cached_raster, GS_DYNAMIC);
	}
	else
	{
		gs_texture_set_image(context->texture, context->cached_raster, context->cached_rasterrowsizeinbytes, false);
	}

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
	DARRAY(char *) arguments;

	context->cached_anypagesrendered = false;

	if (context->file_path != NULL)
	{
		char *command_ignored = "gs";
		char *device_type = "-sDEVICE=display";
		struct dstr display_format_buffer = { 0 };
		struct dstr display_handle_buffer = { 0 };
		struct dstr page_list_buffer = { 0 };
		char *file_flag = "-f";

		dstr_printf(&display_format_buffer, "-dDisplayFormat=%d", DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST |
			DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_TOPFIRST);
		dstr_printf(&display_handle_buffer, "-sDisplayHandle=16#%"PRIx64"", (uint64_t)context);
		dstr_printf(&page_list_buffer, "-sPageList=%d", context->page_number);

		da_init(arguments);

		da_push_back(arguments, &command_ignored);
		da_push_back(arguments, &device_type);
		da_push_back(arguments, &display_handle_buffer.array);
		da_push_back(arguments, &display_format_buffer.array);
		da_push_back(arguments, &page_list_buffer.array);

		if (context->should_override_page_size)
		{
			char *fixed_media = "-dFIXEDMEDIA";
			struct dstr width_buffer = { 0 };
			struct dstr height_buffer = { 0 };

			dstr_printf(&width_buffer, "-dDEVICEWIDTHPOINTS=%d", context->override_width);
			dstr_printf(&height_buffer, "-dDEVICEHEIGHTPOINTS=%d", context->override_height);

			da_push_back(arguments, &fixed_media);

			if (context->override_fit_to_page)
			{
				char *fit_page = "-dPDFFitPage";
				da_push_back(arguments, &fit_page);
			}

			da_push_back(arguments, &width_buffer.array);
			da_push_back(arguments, &height_buffer.array);
		}

		if (context->should_override_dpi)
		{
			struct dstr dpi_buffer = { 0 };

			dstr_printf(&dpi_buffer, "-r%d", context->override_dpi);
			da_push_back(arguments, &dpi_buffer.array);
		}

		da_push_back(arguments, &file_flag);
		da_push_back(arguments, &context->file_path);

		// Here, we execute the Ghostscript command to parse the file and render the document. The display device
		// callbacks indicated above will handle copying the Ghostscript buffer into an OBS texture if any page is
		// rendered. 
		gsapi_init_with_args(shared_ghostscript_instance, (int)arguments.num, arguments.array);
		gsapi_exit(shared_ghostscript_instance);

		da_free(arguments);
	}

	if (!context->cached_anypagesrendered && context->texture != NULL)
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


static bool pdf_source_hotkey_prev(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	if (pressed)
	{
		pdf_source_change_page(data, true);
	}

	return true;
}

static bool pdf_source_hotkey_next(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	if (pressed)
	{
		pdf_source_change_page(data, false);
	}

	return true;
}

static bool pdf_source_override_size_changed(obs_properties_t *props,
	obs_property_t *property, obs_data_t *settings)
{
	bool should_override_size = obs_data_get_bool(settings, "should_override_page_size");

	obs_property_set_visible(obs_properties_get(props, "override_width"), should_override_size);
	obs_property_set_visible(obs_properties_get(props, "override_height"), should_override_size);
	obs_property_set_visible(obs_properties_get(props, "override_fit_to_page"), should_override_size);

	return true;
}

static bool pdf_source_override_dpi_changed(obs_properties_t *props,
	obs_property_t *property, obs_data_t *settings)
{
	bool should_override_dpi = obs_data_get_bool(settings, "should_override_dpi");

	obs_property_set_visible(obs_properties_get(props, "override_dpi"), should_override_dpi);

	return true;
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

	context->should_override_page_size = obs_data_get_bool(settings, "should_override_page_size");
	context->override_width = (int)obs_data_get_int(settings, "override_width");
	context->override_height = (int)obs_data_get_int(settings, "override_height");
	context->override_fit_to_page = obs_data_get_bool(settings, "override_fit_to_page");

	context->should_override_dpi = obs_data_get_bool(settings, "should_override_dpi");
	context->override_dpi = (int)obs_data_get_int(settings, "override_dpi");

	pdf_source_load(context);
}

static void *pdf_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct pdf_source *context = bzalloc(sizeof(struct pdf_source));
	context->src = source;

	context->change_page_hotkey_pair = obs_hotkey_pair_register_source(source, 
		"PdfSource.PrevPage", obs_module_text("PdfSource.PrevPage"),
		"PdfSource.NextPage", obs_module_text("PdfSource.NextPage"),
		pdf_source_hotkey_prev, pdf_source_hotkey_next, context, context);
	
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

	obs_hotkey_pair_unregister(context->change_page_hotkey_pair);
	context->change_page_hotkey_pair = OBS_INVALID_HOTKEY_PAIR_ID;

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

	obs_property_t *should_override_page_size_prop = obs_properties_add_bool(props, "should_override_page_size",
		obs_module_text("PdfSource.ShouldOverridePageSize"));
	obs_properties_add_int(props, "override_width", obs_module_text("PdfSource.OverridePageSize.Width"), 1, INT_MAX, 1);
	obs_properties_add_int(props, "override_height", obs_module_text("PdfSource.OverridePageSize.Height"), 1, INT_MAX, 1);
	obs_properties_add_bool(props, "override_fit_to_page", obs_module_text("PdfSource.OverridePageSize.FitToPage"));

	obs_property_t *should_override_dpi_prop = obs_properties_add_bool(props, "should_override_dpi",
		obs_module_text("PdfSource.ShouldOverrideDpi"));
	obs_properties_add_int(props, "override_dpi", obs_module_text("PdfSource.OverrideDpi"), 1, 600, 1);

	obs_property_set_modified_callback(should_override_page_size_prop, pdf_source_override_size_changed);
	obs_property_set_modified_callback(should_override_dpi_prop, pdf_source_override_dpi_changed);

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

static void pdf_source_key_click(void *data,
	const struct obs_key_event *event, bool key_up)
{
	struct pdf_source *context = data;

	if (!key_up)
	{
		enum obs_key_t key = obs_key_from_virtual_key(event->native_vkey);

		switch (key)
		{
			case OBS_KEY_UP:
			case OBS_KEY_PAGEUP:
				pdf_source_change_page(context, true);
				break;

			case OBS_KEY_DOWN:
			case OBS_KEY_PAGEDOWN:
				pdf_source_change_page(context, false);
				break;
		}
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
	obs_data_set_default_bool(settings, "should_override_page_size", false);
	obs_data_set_default_bool(settings, "override_fit_to_page", true);
	obs_data_set_default_bool(settings, "should_override_dpi", false);
	obs_data_set_default_int(settings, "override_dpi", 72);
}

struct obs_source_info pdf_source_info = {
	.id             = "obs_ghostscript",
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
	.key_click      = pdf_source_key_click,
	.mouse_wheel    = pdf_source_mouse_wheel
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ghostscript", "en-US")

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
