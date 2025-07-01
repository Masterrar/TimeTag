
/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static FILE *log_file = NULL;
static char *video_filepath = NULL; // строка с путем к видео (динамическая)
static double recording_start_time = 0;

// Получить текущее время в секундах с точностью до миллисекунд
static double get_time_seconds()
{
#if defined(_WIN32)
	LARGE_INTEGER freq, counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart / freq.QuadPart;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

static void create_md_log_file(const char *filepath)
{
	if (!filepath)
		return;

	// Сохраняем копию пути в глобальную переменную
	free(video_filepath);
	video_filepath = strdup(filepath);
	const char *ext = strrchr(filepath, '.');
	if (!ext)
		ext = filepath + strlen(filepath);

	size_t md_path_len = ext - filepath + 4 + 1; // ".md" + '\0'
	char *md_path = (char *)malloc(md_path_len);
	if (!md_path)
		return;

	strncpy(md_path, filepath, ext - filepath);
	md_path[ext - filepath] = '\0';
	strcat(md_path, ".md");

	if (log_file) {
		fclose(log_file);
		log_file = NULL;
	}

	log_file = fopen(md_path, "w");
	if (!log_file) {
		// Не пишем в лог, просто освобождаем
		free(md_path);
		return;
	}

	// Не закрываем файл, оставляем открытым
	free(md_path);
}

static void on_recording_started(void *unused, calldata_t *cd)
{
	obs_output_t *output = obs_frontend_get_recording_output();
 // Предполагается, что output уже получен

	obs_data_t *settings = obs_output_get_settings(output);
	if (settings) {
		const char *filepath = obs_data_get_string(settings, "path");
		if (filepath) {
			// Используйте filepath (например, скопируйте его или сохраните)
			obs_log(LOG_INFO, "Path to fileи: %s", filepath);
			create_md_log_file(filepath);
			recording_start_time = get_time_seconds();
		}
		obs_data_release(settings);
	}

}

static void on_recording_stopping(void *unused, calldata_t *cd)
{
	if (log_file) {
		fclose(log_file);
		log_file = NULL;
	}
	if (video_filepath) {
		free(video_filepath);
		video_filepath = NULL;
	}
	recording_start_time = 0.0;
	obs_log(LOG_INFO, "Recording stopped, log file closed");
}
static void on_hotkey_pressed(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	if (pressed && log_file && video_filepath && recording_start_time != 0.0) {
		double now = get_time_seconds();
		double elapsed = now - recording_start_time;

		int total_ms = (int)(elapsed * 1000);
		int minutes = total_ms / 60000;
		int seconds = (total_ms % 60000) / 1000;
		int milliseconds = (total_ms % 1000) / 10; // 2 знака миллисекунд

		// Формируем строку
		// Пример: [04:55](file:///C:/Videos/video.mp4#t=04:55.73)
		fprintf(log_file, "[%02d:%02d](file:///%s#t=%02d:%02d.%02d)\n", minutes, seconds, video_filepath,
			minutes, seconds, milliseconds);
		fflush(log_file);
	}
}
static void on_event_recording_callback(enum obs_frontend_event event, void *data)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		on_recording_started(NULL, NULL);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		on_recording_stopping(NULL, NULL);
		break;
	default:
		break;
	}
}
bool obs_module_load(void)
{
	obs_hotkey_register_frontend("time-tag.tag", "TimeTag Tag Hotkey", on_hotkey_pressed, NULL);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	obs_frontend_add_event_callback(on_event_recording_callback,NULL);

	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
