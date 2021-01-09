#include "stdafx.h"
#include "EliteAfDetector.h"
#include "ui/ArtefactDetectorUI.h"
//#include "../Include/xrRender/Kinematics.h"
//#include "../LightAnimLibrary.h"
#include "player_hud.h"
//#include "clsid_game.h"
#include "artifact.h"
//#include "../Include/xrRender/UIRender.h"
#include "ui/UIXmlInit.h"
//#include "ui/xrUIXmlParser.h"
//#include "ui/UIStatic.h"

CEliteDetector::CEliteDetector()
{
}

CEliteDetector::~CEliteDetector()
{
}

void CEliteDetector::CreateUI()
{
	R_ASSERT(NULL == m_ui);
	m_ui = new CUIArtefactDetectorElite();
	ui().construct(this);

	if (for_test == 1){
		Fvector						P;
		P.set(this->Position());
		feel_touch_update(P, fortestrangetocheck);


		for (xr_vector<CObject*>::iterator it = feel_touch.begin(); it != feel_touch.end(); it++)
		{
			CObject* item = *it;
			if (item){
				Fvector dir, to;

				CInventoryItem* invitem = smart_cast<CInventoryItem*>(item);
				item->Center(to);
				float range = dir.sub(to, this->Position()).magnitude();
				Msg("Artefact %s, distance is %f", invitem->object().cNameSect().c_str(), range);
			}
		}
	}
}

//----------------------------------------------------------------------
void CEliteDetector::feel_touch_new(CObject* O)
{
}

void CEliteDetector::feel_touch_delete(CObject* O)
{
}

BOOL CEliteDetector::feel_touch_contact(CObject *O)
{

	bool is_visibleforDetector = false;
	CArtefact* artefact = smart_cast<CArtefact*>(O);

	if (artefact){
		for (size_t id = 0; id < af_types.size(); ++id) {

			if ((xr_strcmp(O->cNameSect(), af_types[id]) == 0) || (xr_strcmp(af_types[id], "all") == 0)) {
				is_visibleforDetector = true;
			}
		}
		if (is_visibleforDetector == true) {
			return true;
		}
		else{ return false; }
	}
	else{ return false; }

}

//----------------------------------------------------------------------

void CEliteDetector::UpdateAf()
{
	ui().Clear();
	CArtefact* pCurrentAf;
	LPCSTR closest_art = "null";
	feel_touch_update_delay = feel_touch_update_delay + 1;
	if (feel_touch_update_delay >= 5) //��� �� ������� �������� �� ���� �������
	{
		feel_touch_update_delay = 0;
		Fvector						P;
		P.set(this->Position());
		feel_touch_update(P, foverallrangetocheck);
	}

	float disttoclosestart = 0.0;

	CArtefact* arttemp;
	for (xr_vector<CObject*>::iterator it = feel_touch.begin(); it != feel_touch.end(); it++)
	{

		float disttoart = DetectorFeel(*it);
		if (disttoart != -10.0)
		{
			//-----------��� ����, � ������� ��������, �� ������ ��� ��� - ���� �� �����
			arttemp = smart_cast<CArtefact*>(*it);
			ui().RegisterItemToDraw(arttemp->Position(), "af_sign");

			//���� ���������� ����� ��� �� �������(������ ������ �����), �� ���� �� ��������
			if (disttoclosestart <= 0.0)
			{
				disttoclosestart = disttoart;
				closest_art = closestart;
				pCurrentAf = arttemp;
			}
			//����� ����� ������� ���...
			if (disttoclosestart > disttoart)
			{
				disttoclosestart = disttoart;
				closest_art = closestart;
				pCurrentAf = arttemp;
			}
		}
	}
	
	if (reaction_sound_off == 0){
		//���������� ������� ������� ������������ �������
		if (disttoclosestart == 0.0)
		{
			return;
		}
		else{
			cur_periodperiod = disttoclosestart / fdetect_radius * pCurrentAf->detect_radius_koef;
		}

		//����� �� ����������� ����. ������
		if (cur_periodperiod < 0.11)
		{
			cur_periodperiod = 0.11;
		}

		//������� ����������� ������ ������ ����� ��� ��������� �����
		if (xr_strcmp(af_sounds_section, "null") != 0){
			HUD_SOUND_ITEM::LoadSound(closest_art, af_sounds_section, sound, SOUND_TYPE_ITEM);
			LPCSTR check = READ_IF_EXISTS(pSettings, r_string, closest_art, af_sounds_section, "null");
			if (xr_strcmp(check, "null") != 0){
				detect_sndsnd = sound;
			}

		}
		//Msg("dist to art %f", disttoclosestart);
		//Msg("cur_period %f", cur_periodperiod);
		if (snd_timetime > cur_periodperiod)
		{
			snd_timetime = 0;

			HUD_SOUND_ITEM::PlaySound(detect_sndsnd, Fvector().set(0, 0, 0), this, true, false);
			if (detect_sndsnd.m_activeSnd){

				//float freq = 12.0 - (cur_periodperiod * 12.0);

				float procent_cur_periodperiod = (1.0 - 0.11) / 100;
				float procent_freq = (1.8 - 0.8) / 100;
				float freqq = 1.8 - (procent_freq * ((cur_periodperiod - 0.11) / procent_cur_periodperiod));

				if (freqq < 0.8)
					freqq = 0.8;

				//Msg("freqq = %f", freqq);
				detect_sndsnd.m_activeSnd->snd.set_frequency(freqq);
			}
		}
		else
			snd_timetime += Device.fTimeDelta;
	}
}

//������ ��� �������� ����� � ��������� �������, � �� � ��� ������ ���

float CEliteDetector::DetectorFeel(CObject* item)
{
	Fvector dir, to;

	item->Center(to);
	float range = dir.sub(to, Position()).magnitude();
	CInventoryItem* invitem = smart_cast<CInventoryItem*>(item);
	CArtefact* artefact = smart_cast<CArtefact*>(item);

	float gogogo = fdetect_radius *  artefact->detect_radius_koef;
	if (range<gogogo)
	{
		//Msg("feeldetector  %s  %s", invitem->object().cNameSect_str(), invitem->object().cNameSect().c_str());
		closestart = invitem->object().cNameSect_str();
		return range;
	}
	return -10.0;
}

//---------------UI(�������� �����)------------------

bool  CEliteDetector::render_item_3d_ui_query()
{
	return IsWorking();
}

void CEliteDetector::render_item_3d_ui()
{
	R_ASSERT(HudItemData());
	inherited::render_item_3d_ui();
	ui().Draw();
	//	Restore cull mode
	UIRender->CacheSetCullMode(IUIRender::cmCCW);
}

void fix_ws_wnd_size(CUIWindow* w, float kx)
{
	Fvector2 p = w->GetWndSize();
	p.x /= kx;
	w->SetWndSize(p);

	p = w->GetWndPos();
	p.x /= kx;
	w->SetWndPos(p);

	CUIWindow::WINDOW_LIST::iterator it = w->GetChildWndList().begin();
	CUIWindow::WINDOW_LIST::iterator it_e = w->GetChildWndList().end();

	for (; it != it_e; ++it)
	{
		CUIWindow* w2 = *it;
		fix_ws_wnd_size(w2, kx);
	}
}

CUIArtefactDetectorElite&  CEliteDetector::ui()
{
	return *((CUIArtefactDetectorElite*)m_ui);
}


void CUIArtefactDetectorElite::construct(CEliteDetector* p)
{
	m_parent = p;
	CUIXml								uiXml;
	uiXml.Load(CONFIG_PATH, UI_PATH, "ui_detector_artefact.xml");

	CUIXmlInit							xml_init;
	string512							buff;
	xr_strcpy(buff, p->ui_xml_tag());

	xml_init.InitWindow(uiXml, buff, 0, this);

	m_wrk_area = new CUIWindow();

	xr_sprintf(buff, "%s:wrk_area", p->ui_xml_tag());

	xml_init.InitWindow(uiXml, buff, 0, m_wrk_area);
	m_wrk_area->SetAutoDelete(true);
	AttachChild(m_wrk_area);
	xr_sprintf(buff, "%s", p->ui_xml_tag());
	int num = uiXml.GetNodesNum(buff, 0, "palette");
	XML_NODE* pStoredRoot = uiXml.GetLocalRoot();
	uiXml.SetLocalRoot(uiXml.NavigateToNode(buff, 0));
	for (int idx = 0; idx<num; ++idx)
	{
		CUIStatic* S = new CUIStatic();
		shared_str name = uiXml.ReadAttrib("palette", idx, "id");
		m_palette[name] = S;
		xml_init.InitStatic(uiXml, "palette", idx, S);
		S->SetAutoDelete(true);
		m_wrk_area->AttachChild(S);
		S->SetCustomDraw(true);
	}
	uiXml.SetLocalRoot(pStoredRoot);

	Fvector _map_attach_p = pSettings->r_fvector3(m_parent->cNameSect(), "ui_p");
	Fvector _map_attach_r = pSettings->r_fvector3(m_parent->cNameSect(), "ui_r");

	_map_attach_r.mul(PI / 180.f);
	m_map_attach_offset.setHPB(_map_attach_r.x, _map_attach_r.y, _map_attach_r.z);
	m_map_attach_offset.translate_over(_map_attach_p);
}

void CUIArtefactDetectorElite::update()
{
	inherited::update();
	CUIWindow::Update();
}

void CUIArtefactDetectorElite::Draw()
{

	Fmatrix						LM;
	GetUILocatorMatrix(LM);

	IUIRender::ePointType bk = UI().m_currentPointType;

	UI().m_currentPointType = IUIRender::pttLIT;

	UIRender->CacheSetXformWorld(LM);
	UIRender->CacheSetCullMode(IUIRender::cmNONE);

	CUIWindow::Draw();

	//.	Frect r						= m_wrk_area->GetWndRect();
	Fvector2 wrk_sz = m_wrk_area->GetWndSize();
	Fvector2					rp;
	m_wrk_area->GetAbsolutePos(rp);

	Fmatrix						M, Mc;
	float h, p;
	Device.vCameraDirection.getHP(h, p);
	Mc.setHPB(h, 0, 0);
	Mc.c.set(Device.vCameraPosition);
	M.invert(Mc);

	UI().ScreenFrustumLIT().CreateFromRect(Frect().set(rp.x,
		rp.y,
		wrk_sz.x,
		wrk_sz.y));

	xr_vector<SDrawOneItem>::const_iterator it = m_items_to_draw.begin();
	xr_vector<SDrawOneItem>::const_iterator it_e = m_items_to_draw.end();
	for (; it != it_e; ++it)
	{
		Fvector					p = (*it).pos;
		Fvector					pt3d;
		M.transform_tiny(pt3d, p);
		float kz = wrk_sz.y / m_parent->fdetect_radius;
		pt3d.x *= kz;
		pt3d.z *= kz;

		pt3d.x += wrk_sz.x / 2.0f;
		pt3d.z -= wrk_sz.y;

		Fvector2				pos;
		pos.set(pt3d.x, -pt3d.z);
		pos.sub(rp);
		if (1 /* r.in(pos)*/)
		{
			(*it).pStatic->SetWndPos(pos);
			(*it).pStatic->Draw();
		}
	}

	UI().m_currentPointType = bk;
}

void CUIArtefactDetectorElite::GetUILocatorMatrix(Fmatrix& _m)
{
	Fmatrix	trans = m_parent->HudItemData()->m_item_transform;
	u16 bid = m_parent->HudItemData()->m_model->LL_BoneID("cover");
	Fmatrix cover_bone = m_parent->HudItemData()->m_model->LL_GetTransform(bid);
	_m.mul(trans, cover_bone);
	_m.mulB_43(m_map_attach_offset);
}


void CUIArtefactDetectorElite::Clear()
{
	m_items_to_draw.clear();
}

void CUIArtefactDetectorElite::RegisterItemToDraw(const Fvector& p, const shared_str& palette_idx)
{
	xr_map<shared_str, CUIStatic*>::iterator it = m_palette.find(palette_idx);
	if (it == m_palette.end())
	{
		Msg("! RegisterItemToDraw. static not found for [%s]", palette_idx.c_str());
		return;
	}
	CUIStatic* S = m_palette[palette_idx];
	SDrawOneItem				itm(S, p);
	m_items_to_draw.push_back(itm);
}