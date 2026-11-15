#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <psp2/camera.h>
#include <quirc.h>

#include "common.h"
#include "dialog.h"
#include "qr_menu.h"

using namespace paf;

namespace {
	const int kWidth = 640;
	const int kHeight = 360;

	void QrTask(void *userdata)
	{
		static_cast<QrMenu *>(userdata)->Update();
	}
}

QrMenu::QrMenu(ui::Widget *parentRoot, ResultCallback cb, void *userdata) : m_parentRoot(parentRoot), m_pixels(NULL), m_decoder(NULL), m_code(NULL), m_data(NULL),
	m_frame(0), m_open(false), m_closing(false), m_cb(cb), m_userdata(userdata)
{
	Plugin::PageOpenParam param;
	param.graphics_flag = 0x80;
	param.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_qr, param);
	m_parentRoot->SetActivate(false);
	m_cameraPlane = m_root->FindChild(plane_camera_page_qr);
	m_status = m_root->FindChild(text_status_page_qr);
	m_status->SetString(g_appPlugin->GetString(msg_qr_scanning));

	m_pixels = (uint32_t *)memalign(64, kWidth * kHeight * 4);
	m_decoder = quirc_new();
	m_code = (quirc_code *)calloc(1, sizeof(quirc_code));
	m_data = (quirc_data *)calloc(1, sizeof(quirc_data));
	if (!m_pixels || !m_decoder || !m_code || !m_data || quirc_resize(m_decoder, kWidth, kHeight) < 0)
	{
		m_status->SetString(g_appPlugin->GetString(msg_qr_camera_error));
		return;
	}
	memset(m_pixels, 0, kWidth * kHeight * 4);
	m_surface = new graph::Surface(gutil::GetDefaultSurfacePool(), kWidth, kHeight,
		ImageMode_RGBA8888, ImageOrder_Linear, 1, 1, 0);

	memset(&m_info, 0, sizeof(m_info));
	m_info.size = sizeof(m_info);
	m_info.format = SCE_CAMERA_FORMAT_ABGR;
	m_info.resolution = SCE_CAMERA_RESOLUTION_640_360;
	m_info.framerate = 30;
	m_info.pitch = 0;
	m_info.sizeIBase = kWidth * kHeight * 4;
	m_info.pIBase = m_pixels;
	memset(&m_read, 0, sizeof(m_read));
	m_read.size = sizeof(m_read);

	int32_t ret = sceCameraOpen(SCE_CAMERA_DEVICE_BACK, &m_info);
	if (ret >= 0) ret = sceCameraStart(SCE_CAMERA_DEVICE_BACK);
	if (ret < 0)
	{
		m_status->SetString(g_appPlugin->GetString(msg_qr_camera_error));
		return;
	}
	m_open = true;
	m_cameraPlane->SetTexture(m_surface, 0, 0);
	common::MainThreadCallList::Register(QrTask, this);

	ui::Widget *back = m_root->FindChild(button_back_page_qr);
	back->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	back->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		static_cast<QrMenu *>(userdata)->Close();
	}, this);
}

QrMenu::~QrMenu()
{
	common::MainThreadCallList::Unregister(QrTask, this);
	if (m_open)
	{
		sceCameraStop(SCE_CAMERA_DEVICE_BACK);
		sceCameraClose(SCE_CAMERA_DEVICE_BACK);
	}
	if (m_decoder) quirc_destroy(m_decoder);
	free(m_code);
	free(m_data);
	free(m_pixels);
	Plugin::PageCloseParam param;
	param.fade = true;
	param.fade_time_ms = 150.0f;
	g_appPlugin->PageClose(page_qr, param);
	m_parentRoot->SetActivate(true);
}

void QrMenu::Update()
{
	if (!m_open || m_closing || sceCameraRead(SCE_CAMERA_DEVICE_BACK, &m_read) < 0) return;
	m_surface->Copy(0, m_pixels, ImageOrder_Linear, kWidth * 4, 0);
	if (++m_frame % 10) return;

	int w = 0, h = 0;
	uint8_t *gray = quirc_begin(m_decoder, &w, &h);
	for (int i = 0; i < w * h; i++)
	{
		uint32_t pixel = m_pixels[i];
		gray[i] = (uint8_t)(((pixel & 0xff) + ((pixel >> 8) & 0xff) + ((pixel >> 16) & 0xff)) / 3);
	}
	quirc_end(m_decoder);
	const int count = quirc_count(m_decoder);
	if (count <= 0 || count > 64 || !m_code || !m_data) return;
	memset(m_code, 0, sizeof(*m_code));
	memset(m_data, 0, sizeof(*m_data));
	quirc_extract(m_decoder, 0, m_code);
	if (m_code->size <= 0 || m_code->size > QUIRC_MAX_GRID_SIZE) return;
	if (quirc_decode(m_code, m_data) == QUIRC_SUCCESS && m_data->payload_len > 0 && m_data->payload_len <= QUIRC_MAX_PAYLOAD)
	{
		int length = m_data->payload_len < 2047 ? m_data->payload_len : 2047;
		char payload[2048];
		memcpy(payload, m_data->payload, length);
		payload[length] = 0;
		Finish(payload);
	}
}

void QrMenu::Finish(const char *payload)
{
	if (m_closing) return;
	m_closing = true;
	m_result = payload;
	common::MainThreadCallList::Unregister(QrTask, this);
	if (m_open)
	{
		sceCameraStop(SCE_CAMERA_DEVICE_BACK);
		sceCameraClose(SCE_CAMERA_DEVICE_BACK);
		m_open = false;
	}
	DeferCall([](void *ptr)
	{
		QrMenu *menu = static_cast<QrMenu *>(ptr);
		paf::string result = menu->m_result;
		ResultCallback cb = menu->m_cb;
		void *userdata = menu->m_userdata;
		delete menu;
		if (cb) cb(result, userdata);
	}, this);
}

void QrMenu::Close()
{
	if (m_closing) return;
	m_closing = true;
	common::MainThreadCallList::Unregister(QrTask, this);
	if (m_open)
	{
		sceCameraStop(SCE_CAMERA_DEVICE_BACK);
		sceCameraClose(SCE_CAMERA_DEVICE_BACK);
		m_open = false;
	}
	DeferDelete(this);
}
