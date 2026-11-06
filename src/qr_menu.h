#ifndef _FM_QR_MENU_H_
#define _FM_QR_MENU_H_

#include <sdk_compat.h>
#include <paf.h>
#include <psp2/camera.h>

struct quirc;

/** Native ScePaf QR scanner using the rear camera and quirc decoder. */
class QrMenu
{
public:
	typedef void (*ResultCallback)(const paf::string &payload, void *userdata);
	QrMenu(paf::ui::Widget *parentRoot, ResultCallback cb, void *userdata);
	~QrMenu();
	void Update();
	void Close();

private:
	void Finish(const char *payload);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_cameraPlane;
	paf::ui::Widget *m_status;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	uint32_t *m_pixels;
	quirc *m_decoder;
	SceCameraInfo m_info;
	SceCameraRead m_read;
	uint32_t m_frame;
	bool m_open;
	bool m_closing;
	paf::string m_result;
	ResultCallback m_cb;
	void *m_userdata;
};

#endif
