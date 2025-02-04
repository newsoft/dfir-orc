//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//

#include "stdafx.h"

// GETSAMPLES

#include "ConfigFile_GetSamples.h"

using namespace Orc;
using namespace Orc::Config;
using namespace Orc::Config::Common;

// SAMPLES node
HRESULT samples(ConfigItem& parent, DWORD dwIndex)
{
    HRESULT hr = E_FAIL;
    if (FAILED(hr = parent.AddChildNode(L"samples", dwIndex, ConfigItem::MANDATORY)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"MaxTotalBytes", CONFIG_MAXBYTESTOTAL, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"MaxPerSampleBytes", CONFIG_MAXBYTESPERSAMPLE, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"MaxSampleCount", CONFIG_MAXSAMPLECOUNT, ConfigItem::OPTION)))
        return hr;
    return S_OK;
}

// GETTHIS node
HRESULT getthis(ConfigItem& parent, DWORD dwIndex)
{
    HRESULT hr = E_FAIL;
    if (FAILED(hr = parent.AddChildNode(L"getthis", dwIndex, ConfigItem::MANDATORY)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"name", GETSTAMPLES_GETTHIS_EXENAME, ConfigItem::MANDATORY)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"run", GETSTAMPLES_GETTHIS_EXERUN, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"run32", GETSTAMPLES_GETTHIS_EXERUN32, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"run64", GETSTAMPLES_GETTHIS_EXERUN64, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = parent[dwIndex].AddAttribute(L"args", GETSTAMPLES_GETTHIS_ARGS, ConfigItem::OPTION)))
        return hr;
    return S_OK;
}

HRESULT Orc::Config::GetSamples::root(ConfigItem& item)
{
    HRESULT hr = E_FAIL;

    item.strName = L"getsamples";
    item.Flags = ConfigItem::MANDATORY;
    item.dwIndex = 0L;
    item.Status = ConfigItem::MISSING;

    if (FAILED(hr = item.AddChild(L"output", output, GETSAMPLES_OUTPUT)))
        return hr;
    if (FAILED(hr = item.AddChild(L"sampleinfo", output, GETSAMPLES_SAMPLEINFO)))
        return hr;
    if (FAILED(hr = item.AddChild(L"timeline", output, GETSAMPLES_TIMELINE)))
        return hr;
    if (FAILED(hr = item.AddChild(L"criterias", output, GETSAMPLES_CRITERIAS)))
        return hr;
    if (FAILED(hr = item.AddChild(getthis, GETSAMPLES_GETTHIS)))
        return hr;
    if (FAILED(hr = item.AddChildNode(L"temporary", GETSAMPLES_TEMPDIR, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = item.AddChild(optional_location, GETSAMPLES_LOCATIONS)))
        return hr;
    if (FAILED(hr = item.AddChild(knownlocations, GETSAMPLES_KNOWNLOCATIONS)))
        return hr;
    if (FAILED(hr = item.AddChild(::samples, GETSAMPLES_SAMPLES)))
        return hr;
    if (FAILED(hr = item.AddChild(logging, GETSAMPLES_LOGGING)))
        return hr;
    if (FAILED(hr = item.AddChild(L"autoruns", output, GETSAMPLES_AUTORUNS)))
        return hr;
    if (FAILED(hr = item.AddAttribute(L"flushregistry", GETSAMPLES_FLUSHREGISTRY, ConfigItem::OPTION)))
        return hr;
    if (FAILED(hr = item.AddAttribute(L"nolimits", GETSAMPLES_NOLIMITS, ConfigItem::OPTION)))
        return hr;
    return S_OK;
}
