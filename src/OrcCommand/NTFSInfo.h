//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//
#pragma once

#include "OrcCommand.h"

#include "Location.h"
#include "LocationOutput.h"

#include "VolumeReader.h"

#include "UtilitiesMain.h"

#include "MFTWalker.h"
#include "NtfsFileInfo.h"

#include "Authenticode.h"

#include "boost/logic/tribool.hpp"

#pragma managed(push, off)

namespace Orc {

class DataAttribute;
class MftRecordAttribute;
class AttributeListEntry;

namespace Command::NTFSInfo {

enum KindOfTime : DWORD
{
    InvalidKind = 0,
    CreationTime = 1,
    LastModificationTime = 1 << 1,
    LastAccessTime = 1 << 2,
    LastChangeTime = 1 << 3,
    FileNameCreationDate = 1 << 4,
    FileNameLastModificationDate = 1 << 5,
    FileNameLastAccessDate = 1 << 6,
    FileNameLastAttrModificationDate = 1 << 7
};

class ORCUTILS_API Main : public UtilitiesMain
{
public:
    class Configuration : public UtilitiesMain::Configuration
    {
    public:
        Configuration(logger pLog)
            : locs(std::move(pLog))
        {
            bGetKnownLocations = false;
            bResurrectRecords = boost::logic::indeterminate;
            bAddShadows = boost::logic::indeterminate;
            bPopSystemObjects = boost::logic::indeterminate;
            bWriteErrorCodes = false;
            ColumnIntentions = FILEINFO_NONE;
            DefaultIntentions = FILEINFO_NONE;

            outFileInfo.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::TableFile | OutputSpec::Kind::Directory | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
            volumesStatsOutput.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::Directory | OutputSpec::Kind::TableFile | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
            outTimeLine.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::TableFile | OutputSpec::Kind::Directory | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
            outAttrInfo.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::TableFile | OutputSpec::Kind::Directory | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
            outI30Info.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::TableFile | OutputSpec::Kind::Directory | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
            outSecDescrInfo.supportedTypes = static_cast<OutputSpec::Kind>(
                OutputSpec::Kind::TableFile | OutputSpec::Kind::Directory | OutputSpec::Kind::Archive
                | OutputSpec::Kind::SQL);
        };

        std::wstring strWalker;

        // Output Specification
        OutputSpec outFileInfo;
        OutputSpec volumesStatsOutput;

        OutputSpec outTimeLine;
        OutputSpec outAttrInfo;
        OutputSpec outI30Info;
        OutputSpec outSecDescrInfo;

        std::wstring strComputerName;

        boost::logic::tribool bGetKnownLocations;

        bool bWriteErrorCodes;

        LocationSet locs;

        boost::logic::tribool bResurrectRecords;
        boost::logic::tribool bAddShadows;
        boost::logic::tribool bPopSystemObjects;

        Intentions ColumnIntentions;
        Intentions DefaultIntentions;
        std::vector<Filter> Filters;
    };

private:
    Configuration config;

    MultipleOutput<LocationOutput> m_FileInfoOutput;
    MultipleOutput<LocationOutput> m_VolStatOutput;

    MultipleOutput<LocationOutput> m_TimeLineOutput;
    MultipleOutput<LocationOutput> m_AttrOutput;
    MultipleOutput<LocationOutput> m_I30Output;
    MultipleOutput<LocationOutput> m_SecDescrOutput;

    MFTWalker::FullNameBuilder m_FullNameBuilder;
    DWORD dwTotalFileTreated;
    DWORD m_dwProgress;

    Authenticode m_codeVerifier;

    HRESULT Prepare();
    HRESULT GetWriters(std::vector<std::shared_ptr<Location>>& locs);
    HRESULT WriteTimeLineEntry(
        ITableOutput& pTimelineOutput,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const PFILE_NAME pFileName,
        DWORD dwKind,
        LONGLONG llTime);

    std::wstring GetWalkerFromConfig(const ConfigItem& config);
    boost::logic::tribool GetResurrectFromConfig(const ConfigItem& config);
    boost::logic::tribool GetPopulateSystemObjectsFromConfig(const ConfigItem& config);

    bool GetKnownLocationFromConfig(const ConfigItem& config);

    HRESULT RunThroughUSNJournal();
    HRESULT RunThroughMFT();

    // USN Walkercallback
    void USNInformation(
        const std::shared_ptr<TableOutput::IWriter>& pWriter,
        const std::shared_ptr<VolumeReader>& volreader,
        WCHAR* szFullName,
        PUSN_RECORD pRecord);

    // MFT Walker call backs
    void DisplayProgress(const ULONG dwProgress);
    void ElementInformation(ITableOutput& output, const std::shared_ptr<VolumeReader>& volreader, MFTRecord* pElt);
    void DirectoryInformation(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const PFILE_NAME pFileName,
        const std::shared_ptr<IndexAllocationAttribute>& pAttr);
    void FileAndDataInformation(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const PFILE_NAME pFileName,
        const std::shared_ptr<DataAttribute>& pDataAttr);
    void AttrInformation(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const AttributeListEntry& pAttr);
    void I30Information(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const PINDEX_ENTRY pEntry,
        PFILE_NAME pFileName,
        bool bCarvedEntry);
    void TimelineInformation(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        MFTRecord* pElt,
        const PFILE_NAME pFileName);
    void SecurityDescriptorInformation(
        ITableOutput& output,
        const std::shared_ptr<VolumeReader>& volreader,
        const PSECURITY_DESCRIPTOR_ENTRY pEntry);

public:
    static LPCWSTR ToolName() { return L"NTFSInfo"; }
    static LPCWSTR ToolDescription() { return L"NTFSInfo - NTFS File system enumeration"; }

    static ConfigItem::InitFunction GetXmlConfigBuilder();
    static LPCWSTR DefaultConfiguration() { return L"res:#NTFSINFO_CONFIG"; }
    static LPCWSTR ConfigurationExtension() { return nullptr; }

    static ConfigItem::InitFunction GetXmlLocalConfigBuilder() { return nullptr; }
    static LPCWSTR LocalConfiguration() { return nullptr; }
    static LPCWSTR LocalConfigurationExtension() { return nullptr; }

    static LPCWSTR DefaultSchema() { return L"res:#NTFSINFO_SQLSCHEMA"; }

    Main(logger pLog)
        : UtilitiesMain(pLog)
        , config(pLog)
        , dwTotalFileTreated(0L)
        , m_dwProgress(0L)
        , m_codeVerifier(pLog)
        , m_FileInfoOutput(pLog)
        , m_VolStatOutput(pLog)
        , m_TimeLineOutput(pLog)
        , m_AttrOutput(pLog)
        , m_I30Output(pLog)
        , m_SecDescrOutput(pLog) {};

    // implemented in NTFSInfo_Output.cpp
    void PrintUsage();
    void PrintParameters();
    void PrintFooter();

    HRESULT GetColumnsAndFiltersFromConfig(const ConfigItem& configitems);
    HRESULT GetSchemaFromConfig(const ConfigItem& schemaitem);
    HRESULT GetConfigurationFromConfig(const ConfigItem& configitem);
    HRESULT GetLocalConfigurationFromConfig(const ConfigItem& configitem)
    {
        return S_OK;
    };  // No Local Configuration supprt
    HRESULT GetConfigurationFromArgcArgv(int argc, const WCHAR* argv[]);

    HRESULT CheckConfiguration();

    HRESULT Run();
};

}  // namespace Command::NTFSInfo
}  // namespace Orc

#pragma managed(pop)
