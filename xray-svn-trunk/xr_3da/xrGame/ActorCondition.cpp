#include "pch_script.h"
#include "actorcondition.h"
#include "actor.h"
#include "actorEffector.h"
#include "inventory.h"
#include "level.h"
#include "sleepeffector.h"
#include "game_base_space.h"
#include "autosave_manager.h"
#include "xrserver.h"
#include "ai_space.h"
#include "script_callback_ex.h"
#include "script_game_object.h"
#include "game_object_space.h"
#include "script_callback_ex.h"
#include "object_broker.h"
#include "weapon.h"
#include "artifact.h"

#define MAX_SATIETY					1.0f
#define START_SATIETY				0.5f

BOOL	GodMode	()	
{ 
	if (GameID() == GAME_SINGLE) 
		return psActorFlags.test(AF_GODMODE); 
	return FALSE;	
}

bool CActor::IsLimping()
{
	return m_entity_condition->IsLimping();
}

CActorCondition::CActorCondition(CActor *object) :
	inherited	(object)
{
	m_fJumpPower				= 0.f;
	m_fStandPower				= 0.f;
	m_fWalkPower				= 0.f;
	m_fJumpWeightPower			= 0.f;
	m_fWalkWeightPower			= 0.f;
	m_fOverweightWalkK			= 0.f;
	m_fOverweightJumpK			= 0.f;
	m_fAccelK					= 0.f;
	m_fSprintK					= 0.f;
	m_fAlcohol					= 0.f;
	m_fSatiety					= 1.0f;
	m_fThirsty					= 1.0f;

	VERIFY						(object);
	m_object					= object;
	m_condition_flags.zero		();

	fHandsHideTime				= -1;
	m_fBoostersAddWeight		= 0;
}

CActorCondition::~CActorCondition(void)
{
}

void CActorCondition::LoadCondition(LPCSTR entity_section)
{
	inherited::LoadCondition(entity_section);

	LPCSTR						section = READ_IF_EXISTS(pSettings,r_string,entity_section,"condition_sect",entity_section);

	m_fJumpPower				= pSettings->r_float(section,"jump_power");
	m_fStandPower				= pSettings->r_float(section,"stand_power");
	m_fWalkPower				= pSettings->r_float(section,"walk_power");
	m_fJumpWeightPower			= pSettings->r_float(section,"jump_weight_power");
	m_fWalkWeightPower			= pSettings->r_float(section,"walk_weight_power");
	m_fOverweightWalkK			= pSettings->r_float(section,"overweight_walk_k");
	m_fOverweightJumpK			= pSettings->r_float(section,"overweight_jump_k");
	m_fAccelK					= pSettings->r_float(section,"accel_k");
	m_fSprintK					= pSettings->r_float(section,"sprint_k");

	//����� ���� � �������� ������ �������� ����� �������� �������
	m_fLimpingHealthBegin		= pSettings->r_float(section,	"limping_health_begin");
	m_fLimpingHealthEnd			= pSettings->r_float(section,	"limping_health_end");
	R_ASSERT					(m_fLimpingHealthBegin<=m_fLimpingHealthEnd);

	m_fLimpingPowerBegin		= pSettings->r_float(section,	"limping_power_begin");
	m_fLimpingPowerEnd			= pSettings->r_float(section,	"limping_power_end");
	R_ASSERT					(m_fLimpingPowerBegin<=m_fLimpingPowerEnd);

	m_fCantWalkPowerBegin		= pSettings->r_float(section,	"cant_walk_power_begin");
	m_fCantWalkPowerEnd			= pSettings->r_float(section,	"cant_walk_power_end");
	R_ASSERT					(m_fCantWalkPowerBegin<=m_fCantWalkPowerEnd);

	m_fCantSprintPowerBegin		= pSettings->r_float(section,	"cant_sprint_power_begin");
	m_fCantSprintPowerEnd		= pSettings->r_float(section,	"cant_sprint_power_end");
	R_ASSERT					(m_fCantSprintPowerBegin<=m_fCantSprintPowerEnd);

	m_fPowerLeakSpeed			= pSettings->r_float(section,"max_power_leak_speed");
	
	m_fV_Alcohol				= pSettings->r_float(section,"alcohol_v");

	m_fV_Satiety				= pSettings->r_float(section,"satiety_v");		
	m_fV_SatietyPower			= pSettings->r_float(section,"satiety_power_v");
	m_fV_SatietyHealth			= pSettings->r_float(section,"satiety_health_v");

	m_fV_Thirsty				= pSettings->r_float(section, "thirsty_v");
	m_fV_ThirstyPower			= pSettings->r_float(section, "thirsty_power_v");
	m_fV_ThirstyHealth			= pSettings->r_float(section, "thirsty_health_v");
	
	m_MaxWalkWeight					= pSettings->r_float(section,"max_walk_weight");
}


//���������� ���������� � ����� �������
#include "HUDManager.h"

void CActorCondition::UpdateCondition()
{

	if (!object().g_Alive())	return;
	if (!object().Local() && m_object != Level().CurrentViewEntity())		return;	
	
	if (fHandsHideTime != -1 && fHandsHideTime < Device.fTimeGlobal){
		//Msg("Return Hands");
		Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, false);
		fHandsHideTime = -1;
	}

	if (GodMode())				return;

	if ((object().mstate_real&mcAnyMove)) {
		ConditionWalk(object().inventory().TotalWeight()/object().inventory().GetMaxWeight(), isActorAccelerated(object().mstate_real,object().IsZoomAimingMode()), (object().mstate_real&mcSprint) != 0);
	}
	else {
		ConditionStand(object().inventory().TotalWeight()/object().inventory().GetMaxWeight());
	};
	
	if( IsGameTypeSingle() ){

		float k_max_power = 1.0f;

		if( true )
		{
			float weight = object().inventory().TotalWeight();

			float base_w = object().MaxCarryWeight();
/*
			CCustomOutfit* outfit	= m_object->GetOutfit();
			if(outfit)
				base_w += outfit->m_additional_weight2;
*/

			k_max_power = 1.0f + _min(weight,base_w)/base_w + _max(0.0f, (weight-base_w)/10.0f);
		}else
			k_max_power = 1.0f;
		
		SetMaxPower		(GetMaxPower() - m_fPowerLeakSpeed*m_fDeltaTime*k_max_power);
	}


	m_fAlcohol		+= m_fV_Alcohol*m_fDeltaTime;
	clamp			(m_fAlcohol,			0.0f,		1.0f);

	if ( IsGameTypeSingle() )
	{	
		CEffectorCam* ce = Actor()->Cameras().GetCamEffector((ECamEffectorType)effAlcohol);
		if	((m_fAlcohol>0.0001f) ){
			if(!ce){
				AddEffector(m_object,effAlcohol, "effector_alcohol", GET_KOEFF_FUNC(this, &CActorCondition::GetAlcohol));
			}
		}else{
			if(ce)
				RemoveEffector(m_object,effAlcohol);
		}

		
		CEffectorPP* ppe = object().Cameras().GetPPEffector((EEffectorPPType)effPsyHealth);
		
		string64			pp_sect_name;
		shared_str ln		= Level().name();
		strconcat			(sizeof(pp_sect_name),pp_sect_name, "effector_psy_health", "_", *ln);
		if(!pSettings->section_exist(pp_sect_name))
			xr_strcpy			(pp_sect_name, "effector_psy_health");

		if	( !fsimilar(GetPsyHealth(), 1.0f, 0.05f) )
		{
			if(!ppe)
			{
				AddEffector(m_object,effPsyHealth, pp_sect_name, GET_KOEFF_FUNC(this, &CActorCondition::GetPsy));
			}
		}else
		{
			if(ppe)
				RemoveEffector(m_object,effPsyHealth);
		}
		if(fis_zero(GetPsyHealth()))
			health() =0.0f;
	};

	UpdateSatiety				();
	UpdateThirsty				();

	//----------
	//����������� �������������:

		xr_vector<Effector>	temp;

		for (int i = 0; i < Effectors.size(); i++){
			//Msg("Effector %i Device.fTimeGlobal %f fEffectorDuration %f, iEffectorAffectedState = %u, fEffectorUseTime = %f", i, Device.fTimeGlobal, Effectors[i].fEffectorDuration, Effectors[i].iEffectorAffectedState, Effectors[i].fEffectorUseTime);

			if (Effectors[i].fEffectorDuration >= Device.fTimeGlobal){

				if (Effectors[i].fEffectorUseTime < Device.fTimeGlobal){	//��� ����, ����� ���������� ���� �� ����������� ��� ����������� �������. �� �������!

					if (Effectors[i].iEffectorAffectedState == 1){
						//Msg("Health %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangeHealth(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 2){
						//Msg("Bleed %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangeBleeding(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 3){
						//Msg("Rad %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangeRadiation(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 4){
						//Msg("Psy %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangePsyHealth(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 5){
						//Msg("Food %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangeSatiety(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 6){
						//Msg("Thirst %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangeThirsty(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 7){
						//Msg("Energy %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
						ChangePower(Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
					else if (Effectors[i].iEffectorAffectedState == 8){
						//Msg("Alkogol %f", Effectors[i].fEffectorRate * 1.0 * Device.fTimeDelta);
					}
				}
			}
			else{
				//Msg("Effect Expired, remove it");
				Effectors[i].bRemove = true;
				m_fBoostersAddWeight -= Effectors[i].fAddWeightBooster;
			}

			if (Effectors[i].bRemove != true){
				temp.push_back(Effectors[i]);
			}
		}

		//Msg("temp size == %i", temp.size());
		Effectors.clear();
		for (int i = 0; i < temp.size(); i++){
			Effectors.push_back(temp[i]);
		}
		//Msg("Effectors size2 == %i", Effectors.size());

	//----------


	inherited::UpdateCondition	();

	if( IsGameTypeSingle() )
		UpdateTutorialThresholds();
}

void CActorCondition::UpdateSatiety()
{
	if (!IsGameTypeSingle()) return;

	if (m_fSatiety > 0.f)
	{
		m_fSatiety -= m_fV_Satiety*m_fDeltaTime;
		if (m_fSatiety <= 0.f && !GodMode() && object().g_Alive()) //skyloader: kill actor
			object().KillEntity(object().ID());
		else
			clamp(m_fSatiety, 0.0f, 1.0f);
	}

	//������� ����������� �������� ������ ���� ��� �������� ���
	if (!m_bIsBleeding)
	{
		m_fDeltaHealth += CanBeHarmed() ? (m_fV_SatietyHealth*(m_fSatiety>0.0f ? 1.f : -1.f)*m_fDeltaTime) : 0.f;
	}

	//������������ ���������� �������������� ���� �� �������
	m_fDeltaPower += m_fV_SatietyPower*(m_fSatiety>0.0f ? 1.f : -1.f)*m_fDeltaTime;
}

void CActorCondition::UpdateThirsty()
{
	if (!IsGameTypeSingle()) return;

	if (m_fThirsty > 0.f)
	{
		m_fThirsty	-= m_fV_Thirsty*m_fDeltaTime;
		if (m_fThirsty <= 0.f && !GodMode() && object().g_Alive()) //skyloader: kill actor
			object().KillEntity(object().ID());
		else
			clamp			(m_fThirsty, 0.0f, 1.0f);
	}
		
	//��������� ����� ����������� �������� ������ ���� ��� �������� ���
	if(!m_bIsBleeding)
	{
		m_fDeltaHealth += CanBeHarmed() ? (m_fV_ThirstyHealth*(m_fThirsty>0.0f ? 1.f : -1.f)*m_fDeltaTime) : 0.f;
	}

	//������������ ���������� �������������� ���� �� �����
	m_fDeltaPower += (m_fV_ThirstyPower*(m_fThirsty>0.0f ? 1.f : -1.f))*m_fDeltaTime;
}


CWound* CActorCondition::ConditionHit(SHit* pHDS)
{
	if (GodMode()) return NULL;
	return inherited::ConditionHit(pHDS);
}

//weight - "��������" ��� �� 0..1
void CActorCondition::ConditionJump(float weight)
{
	float power			=	m_fJumpPower;
	power				+=	m_fJumpWeightPower*weight*(weight>1.f?m_fOverweightJumpK:1.f);
	m_fPower			-=	HitPowerEffect(power);
}
void CActorCondition::ConditionWalk(float weight, bool accel, bool sprint)
{	
	float power			=	m_fWalkPower;
	power				+=	m_fWalkWeightPower*weight*(weight>1.f?m_fOverweightWalkK:1.f);
	power				*=	m_fDeltaTime*(accel?(sprint?m_fSprintK:m_fAccelK):1.f);
	m_fPower			-=	HitPowerEffect(power);
}

void CActorCondition::ConditionStand(float weight)
{	
	float power			= m_fStandPower;
	power				*= m_fDeltaTime;
	m_fPower			-= power;
}


bool CActorCondition::IsCantWalk() const
{
	if(m_fPower< m_fCantWalkPowerBegin)
		m_bCantWalk		= true;
	else if(m_fPower > m_fCantWalkPowerEnd)
		m_bCantWalk		= false;
	return				m_bCantWalk;
}

#include "CustomOutfit.h"

bool CActorCondition::IsCantWalkWeight()
{
	if(IsGameTypeSingle() && !GodMode())
	{
		float max_w				= m_MaxWalkWeight;
		max_w					+= m_fBoostersAddWeight;//boosters additional weight

		CCustomOutfit* outfit	= m_object->GetOutfit();
		if(outfit)
			max_w += outfit->m_additional_weight;

		//tatarinrafa added additional_inventory_weight to artefacts
		for (int it = 0; it < object().inventory().m_belt.size(); ++it)
		{
			CArtefact*	artefact = smart_cast<CArtefact*>(object().inventory().m_belt[it]);
			if (artefact)
			{
				max_w += artefact->m_additional_weight2;
			}
		}

		if( object().inventory().TotalWeight() > max_w )
		{
			m_condition_flags.set			(eCantWalkWeight, TRUE);
			return true;
		}

	}
	m_condition_flags.set					(eCantWalkWeight, FALSE);
	return false;
}

bool CActorCondition::IsCantSprint() const
{
	if(m_fPower< m_fCantSprintPowerBegin)
		m_bCantSprint	= true;
	else if(m_fPower > m_fCantSprintPowerEnd)
		m_bCantSprint	= false;
	return				m_bCantSprint;
}

bool CActorCondition::IsLimping() const
{
	if(m_fPower< m_fLimpingPowerBegin || GetHealth() < m_fLimpingHealthBegin)
		m_bLimping = true;
	else if(m_fPower > m_fLimpingPowerEnd && GetHealth() > m_fLimpingHealthEnd)
		m_bLimping = false;
	return m_bLimping;
}
extern bool g_bShowHudInfo;

void CActorCondition::save(NET_Packet &output_packet)
{
	inherited::save			(output_packet);
	save_data			(m_fAlcohol, output_packet);
	save_data			(m_condition_flags, output_packet);
	save_data			(m_fSatiety, output_packet);
	save_data			(m_fThirsty, output_packet);
	save_data			(m_MaxWalkWeight, output_packet);
	save_data			(m_fBoostersAddWeight, output_packet);

	for (int i = 0; i < Effectors.size(); i++){
		if (Effectors[i].fEffectorDuration > Device.fTimeGlobal){
			Effectors[i].fEffectorDuration	= Effectors[i].fEffectorDuration - Device.fTimeGlobal;
			Effectors[i].fEffectorUseTime	= Effectors[i].fEffectorUseTime - Device.fTimeGlobal;
			//Msg("Save Effector %i Device.fTimeGlobal %f fEffectorDuration %f, fEffectorUseTime = %f", i, Device.fTimeGlobal, Effectors[i].fEffectorDuration, Effectors[i].fEffectorUseTime);
		}
	}

	save_data			(Effectors, output_packet);

	for (int i = 0; i < Effectors.size(); i++){
		if (Effectors[i].fEffectorDuration > 0)		Effectors[i].fEffectorDuration = Effectors[i].fEffectorDuration + Device.fTimeGlobal;
		if (Effectors[i].fEffectorUseTime > 0)		Effectors[i].fEffectorUseTime = Effectors[i].fEffectorUseTime + Device.fTimeGlobal;
	}
}

void CActorCondition::load(IReader &input_packet)
{
	inherited::load		(input_packet);
	load_data			(m_fAlcohol, input_packet);
	load_data			(m_condition_flags, input_packet);
	load_data			(m_fSatiety, input_packet);
	load_data			(m_fThirsty, input_packet);
	load_data			(m_MaxWalkWeight, input_packet);
	load_data			(m_fBoostersAddWeight, input_packet);

	load_data			(Effectors, input_packet);
	for (int i = 0; i < Effectors.size(); i++){
		Effectors[i].fEffectorDuration += Device.fTimeGlobal;
		Effectors[i].fEffectorUseTime += Device.fTimeGlobal;
		//Msg("Load Effector %i Device.fTimeGlobal %f fEffectorDuration %f, fEffectorUseTime = %f", i, Device.fTimeGlobal, Effectors[i].fEffectorDuration, Effectors[i].fEffectorUseTime);
	}
}

void CActorCondition::reinit	()
{
	inherited::reinit	();
	m_bLimping					= false;
	m_fSatiety					= 1.f;
	m_fThirsty					= 1.f;
}

void CActorCondition::ChangeAlcohol	(const float value)
{
	m_fAlcohol += value;
}

void CActorCondition::ChangeSatiety(const float value)
{
	m_fSatiety += value;
	clamp		(m_fSatiety, 0.0f, 1.0f);
}

void CActorCondition::ChangeThirsty(const float value)
{
	m_fThirsty += value;
	clamp(m_fThirsty, 0.0f, 1.0f);
}

void CActorCondition::UpdateTutorialThresholds()
{
	string256						cb_name;
	static float _cPowerThr			= pSettings->r_float("tutorial_conditions_thresholds","power");
	static float _cPowerMaxThr		= pSettings->r_float("tutorial_conditions_thresholds","max_power");
	static float _cBleeding			= pSettings->r_float("tutorial_conditions_thresholds","bleeding");
	static float _cSatiety			= pSettings->r_float("tutorial_conditions_thresholds","satiety");
	static float _cThirsty			= pSettings->r_float("tutorial_conditions_thresholds","thirsty");
	static float _cRadiation		= pSettings->r_float("tutorial_conditions_thresholds","radiation");
	static float _cWpnCondition		= pSettings->r_float("tutorial_conditions_thresholds","weapon_jammed");
	static float _cPsyHealthThr		= pSettings->r_float("tutorial_conditions_thresholds","psy_health");



	bool b = true;
	if(b && !m_condition_flags.test(eCriticalPowerReached) && GetPower()<_cPowerThr){
		m_condition_flags.set			(eCriticalPowerReached, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_critical_power");
	}

	if(b && !m_condition_flags.test(eCriticalMaxPowerReached) && GetMaxPower()<_cPowerMaxThr){
		m_condition_flags.set			(eCriticalMaxPowerReached, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_critical_max_power");
	}

	if(b && !m_condition_flags.test(eCriticalBleedingSpeed) && BleedingSpeed()>_cBleeding){
		m_condition_flags.set			(eCriticalBleedingSpeed, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_bleeding");
	}

	if(b && !m_condition_flags.test(eCriticalSatietyReached) && GetSatiety()<_cSatiety){
		m_condition_flags.set			(eCriticalSatietyReached, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_satiety");
	}

	if (b && !m_condition_flags.test(eCriticalThirstyReached) && GetSatiety()<_cThirsty){
		m_condition_flags.set(eCriticalThirstyReached, TRUE);
		b = false;
		xr_strcpy(cb_name, "_G.on_actor_thirsty");
	}

	if(b && !m_condition_flags.test(eCriticalRadiationReached) && GetRadiation()>_cRadiation){
		m_condition_flags.set			(eCriticalRadiationReached, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_radiation");
	}

	if(b && !m_condition_flags.test(ePhyHealthMinReached) && GetPsyHealth()>_cPsyHealthThr){
//.		m_condition_flags.set			(ePhyHealthMinReached, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_psy");
	}

	if(b && !m_condition_flags.test(eCantWalkWeight)){
//.		m_condition_flags.set			(eCantWalkWeight, TRUE);
		b=false;
		xr_strcpy(cb_name,"_G.on_actor_cant_walk_weight");
	}

	if(b && !m_condition_flags.test(eWeaponJammedReached)&&m_object->inventory().GetActiveSlot()!=NO_ACTIVE_SLOT){
		PIItem item							= m_object->inventory().ItemFromSlot(m_object->inventory().GetActiveSlot());
		CWeapon* pWeapon					= smart_cast<CWeapon*>(item); 
		if(pWeapon&&pWeapon->GetCondition()<_cWpnCondition){
			m_condition_flags.set			(eWeaponJammedReached, TRUE);b=false;
			xr_strcpy(cb_name,"_G.on_actor_weapon_jammed");
		}
	}
	
	if(!b){
		luabind::functor<LPCSTR>			fl;
		R_ASSERT							(ai().script_engine().functor<LPCSTR>(cb_name,fl));
		fl									();
	}
}
