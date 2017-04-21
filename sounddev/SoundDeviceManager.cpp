/*
 * SoundDeviceManager.cpp
 * ----------------------
 * Purpose: Sound device manager class.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"

#include "SoundDeviceManager.h"
#include "SoundDevice.h"

#include "SoundDeviceASIO.h"
#include "SoundDeviceDirectSound.h"
#include "SoundDevicePortAudio.h"
#include "SoundDeviceWaveout.h"
#include "SoundDeviceStub.h"
#if defined(MPT_ENABLE_PULSEAUDIO_FULL)
#include "SoundDevicePulseaudio.h"
#endif // MPT_ENABLE_PULSEAUDIO_FULL
#include "SoundDevicePulseSimple.h"


OPENMPT_NAMESPACE_BEGIN


namespace SoundDevice {

	
struct CompareInfo
{
	std::map<SoundDevice::Type, int> ordering;
	CompareInfo(const std::map<SoundDevice::Type, int> &ordering)
		: ordering(ordering)
	{
		return;
	}
	bool operator () (const SoundDevice::Info &x, const SoundDevice::Info &y)
	{
		return (ordering[x.type] > ordering[y.type])
			|| ((ordering[x.type] == ordering[y.type]) && (x.isDefault && !y.isDefault))
			;
	}
};


template <typename Tdevice>
void Manager::EnumerateDevices(SoundDevice::SysInfo sysInfo)
//----------------------------------------------------------
{
	const auto infos = Tdevice::EnumerateDevices(sysInfo);
	m_SoundDevices.insert(m_SoundDevices.end(), infos.begin(), infos.end());
	for(auto &i : infos)
	{
		SoundDevice::Identifier identifier = i.GetIdentifier();
		if(!identifier.empty())
		{
			m_DeviceFactoryMethods[identifier] = ConstructSoundDevice<Tdevice>;
		}
	}
}


template <typename Tdevice>
SoundDevice::IBase* Manager::ConstructSoundDevice(const SoundDevice::Info &info, SoundDevice::SysInfo sysInfo)
//------------------------------------------------------------------------------------------------------------
{
	return new Tdevice(info, sysInfo);
}


void Manager::ReEnumerate()
//-------------------------
{
	MPT_TRACE();
	m_SoundDevices.clear();
	m_DeviceUnavailable.clear();
	m_DeviceFactoryMethods.clear();
	m_DeviceCaps.clear();
	m_DeviceDynamicCaps.clear();

#ifdef MPT_WITH_PORTAUDIO
	m_PortAudio.Reload();
#endif // MPT_WITH_PORTAUDIO

#if defined(MPT_ENABLE_PULSEAUDIO_FULL)
#if defined(MPT_WITH_PULSEAUDIO)
	if(IsComponentAvailable(m_Pulseaudio))
	{
		EnumerateDevices<Pulseaudio>(GetSysInfo());
	}
#endif // MPT_WITH_PULSEAUDIO
#endif // MPT_ENABLE_PULSEAUDIO_FULL

#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
	if(IsComponentAvailable(m_PulseaudioSimple))
	{
		EnumerateDevices<PulseaudioSimple>(GetSysInfo());
	}
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE

#if MPT_OS_WINDOWS
	if(IsComponentAvailable(m_WaveOut))
	{
		EnumerateDevices<CWaveDevice>(GetSysInfo());
	}
#endif // MPT_OS_WINDOWS

#ifdef MPT_WITH_DSOUND
	// kind of deprecated by now
	if(IsComponentAvailable(m_DirectSound))
	{
		EnumerateDevices<CDSoundDevice>(GetSysInfo());
	}
#endif // MPT_WITH_DSOUND

#ifdef MPT_WITH_ASIO
	if(IsComponentAvailable(m_ASIO))
	{
		EnumerateDevices<CASIODevice>(GetSysInfo());
	}
#endif // MPT_WITH_ASIO

#ifdef MPT_WITH_PORTAUDIO
	if(IsComponentAvailable(m_PortAudio))
	{
		EnumerateDevices<CPortaudioDevice>(GetSysInfo());
	}
#endif // MPT_WITH_PORTAUDIO

#ifndef MPT_BUILD_WINESUPPORT
	{
		EnumerateDevices<SoundDeviceStub>(GetSysInfo());
	}
#endif // !MPT_BUILD_WINESUPPORT

	std::map<SoundDevice::Type, int> typePriorities;
#ifdef MPT_BUILD_WINESUPPORT
	MPT_CONSTANT_IF(true)
	{
		typePriorities[MPT_USTRING("PulseAudio")] = 32;
		typePriorities[MPT_USTRING("PulseAudio-Simple")] = 31;
#ifdef MPT_WITH_PORTAUDIO
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paAL)] = 29;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paALSA)] = 28;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paJACK)] = 19;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paOSS)] = 18;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paAudioScienceHPI)] = 9;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paBeOS)] = -1;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paCoreAudio)] = -2;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paWASAPI)] = -3;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paMME)] = -4;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paDirectSound)] = -5;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paSoundManager)] = -6;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paWDMKS)] = -7;
		typePriorities[mpt::format(MPT_USTRING("PortAudio-%1"))(paASIO)] = -8;
#endif // MPT_WITH_PORTAUDIO
	} else
#endif
	if(GetSysInfo().IsWine && GetSysInfo().WineHostIsLinux && GetSysInfo().WineVersion.IsAtLeast(mpt::Wine::Version(1,8,0)))
	{ // Wine >= 1.8 on Linux
		typePriorities[MPT_USTRING("Wine-Native-PulseAudio")] = 31;
		typePriorities[SoundDevice::TypePORTAUDIO_WASAPI] = 29;
		typePriorities[SoundDevice::TypeWAVEOUT] = 28;
		typePriorities[MPT_USTRING("Wine-Native-PulseAudio-Simple")] = 25;
		typePriorities[SoundDevice::TypeASIO] = 21;
		typePriorities[SoundDevice::TypePORTAUDIO_WMME] = 19;
		typePriorities[MPT_USTRING("Wine-Native-PortAudio-8")] = 9; // ALSA
		typePriorities[SoundDevice::TypePORTAUDIO_WDMKS] = -1;
		typePriorities[SoundDevice::TypeDSOUND] = -2;
		typePriorities[SoundDevice::TypePORTAUDIO_DS] = -3;
	} else if(GetSysInfo().IsWine)
	{ // Wine
		typePriorities[MPT_USTRING("Wine-Native-PulseAudio")] = 32;
		typePriorities[MPT_USTRING("Wine-Native-PulseAudio-Simple")] = 31;
		typePriorities[SoundDevice::TypeDSOUND] = 29;
		typePriorities[SoundDevice::TypeWAVEOUT] = 28;
		typePriorities[SoundDevice::TypePORTAUDIO_WASAPI] = 27;
		typePriorities[SoundDevice::TypeASIO] = 21;
		typePriorities[SoundDevice::TypePORTAUDIO_WMME] = 19;
		typePriorities[SoundDevice::TypePORTAUDIO_DS] = 18;
		typePriorities[MPT_USTRING("Wine-Native-PortAudio-8")] = 9; // ALSA
		typePriorities[SoundDevice::TypePORTAUDIO_WDMKS] = -1;
	} else if(GetSysInfo().WindowsVersion.IsBefore(mpt::Windows::Version::WinXP))
	{ // < WinXP
		typePriorities[SoundDevice::TypeWAVEOUT] = 29;
		typePriorities[SoundDevice::TypeDSOUND] = 28;
		typePriorities[SoundDevice::TypePORTAUDIO_WMME] = 19;
		typePriorities[SoundDevice::TypePORTAUDIO_DS] = 18;
		typePriorities[SoundDevice::TypeASIO] = 1;
		typePriorities[SoundDevice::TypePORTAUDIO_WDMKS] = -1;
		typePriorities[SoundDevice::TypePORTAUDIO_WASAPI] = -2;
	} else if(GetSysInfo().WindowsVersion.IsBefore(mpt::Windows::Version::WinVista))
	{ // WinXP
		typePriorities[SoundDevice::TypeWAVEOUT] = 29;
		typePriorities[SoundDevice::TypeASIO] = 28;
		typePriorities[SoundDevice::TypeDSOUND] = 27;
		typePriorities[SoundDevice::TypePORTAUDIO_WDMKS] = 26;
		typePriorities[SoundDevice::TypePORTAUDIO_WMME] = 19;
		typePriorities[SoundDevice::TypePORTAUDIO_DS] = 17;
		typePriorities[SoundDevice::TypePORTAUDIO_WASAPI] = -1;
	} else
	{ // >=Vista
		typePriorities[SoundDevice::TypeWAVEOUT] = 29;
		typePriorities[SoundDevice::TypePORTAUDIO_WASAPI] = 28;
		typePriorities[SoundDevice::TypeASIO] = 27;
		typePriorities[SoundDevice::TypePORTAUDIO_WDMKS] = 26;
		typePriorities[SoundDevice::TypePORTAUDIO_WMME] = 19;
		typePriorities[SoundDevice::TypeDSOUND] = -1;
		typePriorities[SoundDevice::TypePORTAUDIO_DS] = -2;
	}
	std::stable_sort(m_SoundDevices.begin(), m_SoundDevices.end(), CompareInfo(typePriorities));

	for(auto &i : m_SoundDevices)
	{
		i.extraData[MPT_USTRING("priority")] = mpt::ufmt::dec(typePriorities[i.type]);
	}

	MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING("Sound Devices enumerated:"))());
	for(std::size_t i = 0; i < m_SoundDevices.size(); ++i)
	{
		MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING(" Identifier : %1"))(m_SoundDevices[i].GetIdentifier()));
		MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING("  Type      : %1"))(m_SoundDevices[i].type));
		MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING("  InternalID: %1"))(m_SoundDevices[i].internalID));
		MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING("  API Name  : %1"))(m_SoundDevices[i].apiName));
		MPT_LOG(LogDebug, "sounddev", mpt::format(MPT_USTRING("  Name      : %1"))(m_SoundDevices[i].name));
		for(const auto &extra : m_SoundDevices[i].extraData)
		{
			Log(LogDebug, mpt::format(MPT_USTRING("  Extra Data: %1 = %2"))(extra.first, extra.second));
		}
	}
	
}


SoundDevice::Manager::GlobalID Manager::GetGlobalID(SoundDevice::Identifier identifier) const
//-------------------------------------------------------------------------------------------
{
	for(std::size_t i = 0; i < m_SoundDevices.size(); ++i)
	{
		if(m_SoundDevices[i].GetIdentifier() == identifier)
		{
			return i;
		}
	}
	return ~SoundDevice::Manager::GlobalID();
}


SoundDevice::Info Manager::FindDeviceInfo(SoundDevice::Manager::GlobalID id) const
//--------------------------------------------------------------------------------
{
	MPT_TRACE();
	if(id > m_SoundDevices.size())
	{
		return SoundDevice::Info();
	}
	return m_SoundDevices[id];
}


SoundDevice::Info Manager::FindDeviceInfo(SoundDevice::Identifier identifier) const
//---------------------------------------------------------------------------------
{
	MPT_TRACE();
	if(m_SoundDevices.empty())
	{
		return SoundDevice::Info();
	}
	if(identifier.empty())
	{
		return SoundDevice::Info();
	}
	for(auto &info : *this)
	{
		if(info.GetIdentifier() == identifier)
		{
			return info;
		}
	}
	return SoundDevice::Info();
}


SoundDevice::Info Manager::FindDeviceInfoBestMatch(SoundDevice::Identifier identifier, bool preferSameType)
//---------------------------------------------------------------------------------------------------------
{
	MPT_TRACE();
	if(m_SoundDevices.empty())
	{
		return SoundDevice::Info();
	}
	if(!identifier.empty())
	{ // valid identifier
		for(auto &info : *this)
		{
			if((info.GetIdentifier() == identifier) && !IsDeviceUnavailable(info.GetIdentifier()))
			{ // exact match
				return info;
			}
		}
		const SoundDevice::Type type = ParseType(identifier);
		if(type == TypePORTAUDIO_WASAPI)
		{
			// WASAPI devices might change names if a different connector jack is used.
			// In order to avoid defaulting to wave mapper in that case,
			// just find the first WASAPI device.
			preferSameType = true;
		}
		if(preferSameType)
		{ // find first avilable device of given type
			for(auto &info : *this)
			{
				if((info.type == type) && !IsDeviceUnavailable(info.GetIdentifier()))
				{
					return info;
				}
			}
		}
	}
	for(auto &info : *this)
	{ // find first available device
		if(!IsDeviceUnavailable(info.GetIdentifier()))
		{
			return info;
		}
	}
	// default to first device
	return *begin();
}


bool Manager::OpenDriverSettings(SoundDevice::Identifier identifier, SoundDevice::IMessageReceiver *messageReceiver, SoundDevice::IBase *currentSoundDevice)
//----------------------------------------------------------------------------------------------------------------------------------------------------------
{
	MPT_TRACE();
	bool result = false;
	if(currentSoundDevice && FindDeviceInfo(identifier).IsValid() && (currentSoundDevice->GetDeviceInfo().GetIdentifier() == identifier))
	{
		result = currentSoundDevice->OpenDriverSettings();
	} else
	{
		SoundDevice::IBase *dummy = CreateSoundDevice(identifier);
		if(dummy)
		{
			dummy->SetMessageReceiver(messageReceiver);
			result = dummy->OpenDriverSettings();
		}
		delete dummy;
	}
	return result;
}


SoundDevice::Caps Manager::GetDeviceCaps(SoundDevice::Identifier identifier, SoundDevice::IBase *currentSoundDevice)
//------------------------------------------------------------------------------------------------------------------
{
	MPT_TRACE();
	if(m_DeviceCaps.find(identifier) == m_DeviceCaps.end())
	{
		if(currentSoundDevice && FindDeviceInfo(identifier).IsValid() && (currentSoundDevice->GetDeviceInfo().GetIdentifier() == identifier))
		{
			m_DeviceCaps[identifier] = currentSoundDevice->GetDeviceCaps();
		} else
		{
			SoundDevice::IBase *dummy = CreateSoundDevice(identifier);
			if(dummy)
			{
				m_DeviceCaps[identifier] = dummy->GetDeviceCaps();
			} else
			{
				SetDeviceUnavailable(identifier);
			}
			delete dummy;
		}
	}
	return m_DeviceCaps[identifier];
}


SoundDevice::DynamicCaps Manager::GetDeviceDynamicCaps(SoundDevice::Identifier identifier, const std::vector<uint32> &baseSampleRates, SoundDevice::IMessageReceiver *messageReceiver, SoundDevice::IBase *currentSoundDevice, bool update)
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
{
	MPT_TRACE();
	if((m_DeviceDynamicCaps.find(identifier) == m_DeviceDynamicCaps.end()) || update)
	{
		if(currentSoundDevice && FindDeviceInfo(identifier).IsValid() && (currentSoundDevice->GetDeviceInfo().GetIdentifier() == identifier))
		{
			m_DeviceDynamicCaps[identifier] = currentSoundDevice->GetDeviceDynamicCaps(baseSampleRates);
			if(!currentSoundDevice->IsAvailable())
			{
				SetDeviceUnavailable(identifier);
			}
		} else
		{
			SoundDevice::IBase *dummy = CreateSoundDevice(identifier);
			if(dummy)
			{
				dummy->SetMessageReceiver(messageReceiver);
				m_DeviceDynamicCaps[identifier] = dummy->GetDeviceDynamicCaps(baseSampleRates);
				if(!dummy->IsAvailable())
				{
					SetDeviceUnavailable(identifier);
				}
			} else
			{
				SetDeviceUnavailable(identifier);
			}
			delete dummy;
		}
	}
	return m_DeviceDynamicCaps[identifier];
}


SoundDevice::IBase * Manager::CreateSoundDevice(SoundDevice::Identifier identifier)
//---------------------------------------------------------------------------------
{
	MPT_TRACE();
	const SoundDevice::Info info = FindDeviceInfo(identifier);
	if(!info.IsValid())
	{
		return nullptr;
	}
	if(m_DeviceFactoryMethods.find(identifier) == m_DeviceFactoryMethods.end())
	{
		return nullptr;
	}
	if(!m_DeviceFactoryMethods[identifier])
	{
		return nullptr;
	}
	SoundDevice::IBase *result = m_DeviceFactoryMethods[identifier](info, GetSysInfo());
	if(!result)
	{
		return nullptr;
	}
	if(!result->Init(m_AppInfo))
	{
		delete result;
		result = nullptr;
		return nullptr;
	}
	m_DeviceCaps[identifier] = result->GetDeviceCaps(); // update cached caps
	return result;
}


Manager::Manager(SoundDevice::SysInfo sysInfo, SoundDevice::AppInfo appInfo)
//--------------------------------------------------------------------------
	: m_SysInfo(sysInfo)
	, m_AppInfo(appInfo)
{
	ReEnumerate();
}


Manager::~Manager()
//-----------------
{
	return;
}


namespace Legacy
{
SoundDevice::Info FindDeviceInfo(SoundDevice::Manager &manager, SoundDevice::Legacy::ID id)
{
	if(manager.GetDeviceInfos().empty())
	{
		return SoundDevice::Info();
	}
	SoundDevice::Type type = SoundDevice::Type();
	switch(id & SoundDevice::Legacy::MaskType)
	{
		case SoundDevice::Legacy::TypeWAVEOUT:
			type = SoundDevice::TypeWAVEOUT;
			break;
		case SoundDevice::Legacy::TypeDSOUND:
			type = SoundDevice::TypeDSOUND;
			break;
		case SoundDevice::Legacy::TypeASIO:
			type = SoundDevice::TypeASIO;
			break;
		case SoundDevice::Legacy::TypePORTAUDIO_WASAPI:
			type = SoundDevice::TypePORTAUDIO_WASAPI;
			break;
		case SoundDevice::Legacy::TypePORTAUDIO_WDMKS:
			type = SoundDevice::TypePORTAUDIO_WDMKS;
			break;
		case SoundDevice::Legacy::TypePORTAUDIO_WMME:
			type = SoundDevice::TypePORTAUDIO_WMME;
			break;
		case SoundDevice::Legacy::TypePORTAUDIO_DS:
			type = SoundDevice::TypePORTAUDIO_DS;
			break;
	}
	if(type.empty())
	{	// fallback to first device
		return *manager.begin();
	}
	std::size_t index = static_cast<uint8>(id & SoundDevice::Legacy::MaskIndex);
	std::size_t seenDevicesOfDesiredType = 0;
	for(auto &info : manager)
	{
		if(info.type == type)
		{
			if(seenDevicesOfDesiredType == index)
			{
				if(!info.IsValid())
				{	// fallback to first device
					return *manager.begin();
				}
				return info;
			}
			seenDevicesOfDesiredType++;
		}
	}
	// default to first device
	return *manager.begin();
}
}


} // namespace SoundDevice


OPENMPT_NAMESPACE_END
