//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//

#include "stdafx.h"

#include "ImportData.h"

#include "ConfigFileReader.h"
#include "ConfigFile_Common.h"

#include "ImportAgent.h"
#include "ImportMessage.h"
#include "ImportNotification.h"
#include "LogFileWriter.h"

#include "CommandAgent.h"

#include "SystemDetails.h"

#include <filesystem>

using namespace std;
using namespace concurrency;
namespace fs = std::experimental::filesystem;

using namespace Orc;
using namespace Orc::Command::ImportData;

HRESULT
Main::AddDirectoryForInput(const std::wstring& dir, const InputItem& input, std::vector<ImportItem>& input_paths)
{
    fs::recursive_directory_iterator end_itr;

    fs::path root(dir);
    root = canonical(root);

    auto dir_iter = fs::recursive_directory_iterator(root);

    auto offset = root.wstring().size() + 1;

    if (!config.bResursive)
        dir_iter.disable_recursion_pending();

    for (; dir_iter != end_itr; dir_iter++)
    {
        auto& entry = *dir_iter;

        if (is_regular_file(entry.status()))
        {
            ImportItem item;

            item.InputFile = make_shared<wstring>(entry.path().wstring());
            item.FullName = item.InputFile->substr(offset);
            item.Name = entry.path().filename().wstring();
            item.Definitions = &input.ImportDefinitions;
            item.DefinitionItem = item.DefinitionItemLookup(ImportDefinition::Expand);
            item.Format = item.GetFileFormat();
            item.isToExtract = false;
            item.isToIgnore = false;
            item.isToImport = false;
            item.isToExpand = true;

            if (!input.NameMatch.empty())
            {
                if (SUCCEEDED(CommandAgent::ReversePattern(
                        input.NameMatch,
                        item.Name,
                        item.SystemType,
                        item.FullComputerName,
                        item.ComputerName,
                        item.TimeStamp)))
                {
                    log::Verbose(_L_, L"Input file added: %s\r\n", item.Name.c_str());
                    input_paths.push_back(std::move(item));
                }
                else
                {
                    log::Verbose(
                        _L_, L"Input file %s does not match %s\r\n", item.Name.c_str(), input.NameMatch.c_str());
                }
            }
            else
            {
                log::Verbose(_L_, L"Input file added: %s\r\n", item.Name.c_str());
                input_paths.push_back(item);
            }
        }
    }
    return S_OK;
}

HRESULT Main::AddFileForInput(const std::wstring& file, const InputItem& input, std::vector<ImportItem>& input_paths)
{
    fs::path entry(file);

    if (is_regular_file(entry))
    {
        ImportItem item;

        item.InputFile = make_shared<wstring>(entry.wstring());

        item.FullName = entry.relative_path().wstring();
        item.Name = entry.filename().wstring();
        item.Definitions = &input.ImportDefinitions;
        item.Format = item.GetFileFormat();
        item.isToExtract = true;
        item.isToIgnore = false;
        item.isToImport = false;
        item.isToExpand = false;

        if (!input.NameMatch.empty())
        {
            auto filename = entry.filename().wstring();

            if (SUCCEEDED(CommandAgent::ReversePattern(
                    input.NameMatch,
                    filename,
                    item.SystemType,
                    item.FullComputerName,
                    item.ComputerName,
                    item.TimeStamp)))
            {
                log::Verbose(_L_, L"Input file added: %s\r\n", filename.c_str());
                input_paths.push_back(std::move(item));
            }
            else
            {
                log::Verbose(_L_, L"Input file %s does not match %s\r\n", filename.c_str(), input.NameMatch.c_str());
            }
        }
        else
        {
            log::Verbose(_L_, L"Input file added: %s\r\n", item.Name.c_str());
            input_paths.push_back(std::move(item));
        }
    }
    return S_OK;
}

std::vector<ImportItem> Main::GetInputFiles(const Main::InputItem& input)
{
    HRESULT hr = E_FAIL;

    std::vector<ImportItem> items;

    if (!input.InputDirectory.empty())
    {
        if (FAILED(hr = AddDirectoryForInput(input.InputDirectory, input, items)))
        {
            log::Error(_L_, hr, L"Failed to add input files for %s\r\n", input.InputDirectory.c_str());
        }
    }

    if (!config.strInputDirs.empty())
    {
        for (auto& dir : config.strInputDirs)
        {
            if (FAILED(hr = AddDirectoryForInput(dir, input, items)))
            {
                log::Error(_L_, hr, L"Failed to add input files for %s\r\n", dir.c_str());
            }
        }
    }

    if (!config.strInputFiles.empty())
    {
        for (auto& file : config.strInputFiles)
        {
            if (FAILED(hr = AddFileForInput(file, input, items)))
            {
                log::Error(_L_, hr, L"Failed to add input file %s\r\n", file.c_str());
            }
        }
    }
    return items;
}

HRESULT Main::Run()
{
    HRESULT hr = E_FAIL;

    if (FAILED(hr = LoadWinTrust()))
        return hr;

    auto pWriter = TableOutput::GetWriter(_L_, config.Output);
    if (pWriter == nullptr)
    {
        log::Warning(_L_, E_FAIL, L"No writer for ImportData output: no data about imports will be generated\r\n");
    }

    auto& output = pWriter->GetTableOutput();

    m_importNotification = std::make_unique<call<ImportNotification::Notification>>(
        [this, &output](const ImportNotification::Notification& item) {
            LONG queued = receive(m_importAgent->QueuedItemsCount());

            if (SUCCEEDED(item->GetHR()))
            {
                switch (item->Item().Format)
                {
                    case ImportItem::Envelopped:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (envelopped message) decrypted (%I64d bytes)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().ullBytesExtracted);
                        break;
                    case ImportItem::Archive:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (archive) extracted (%I64d bytes)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().ullBytesExtracted);
                        break;
                    case ImportItem::CSV:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (csv) imported into %s (%I64d lines)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().DefinitionItem->Table.c_str(),
                            item->Item().ullLinesImported);
                        break;
                    case ImportItem::RegistryHive:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (registry hive) imported into %s (%I64d lines)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().DefinitionItem->Table.c_str(),
                            item->Item().ullLinesImported);
                        break;
                    case ImportItem::EventLog:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (event log) imported into %s (%I64d lines)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().DefinitionItem->Table.c_str(),
                            item->Item().ullLinesImported);
                        break;
                    case ImportItem::XML:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (xml) imported (%I64d bytes)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().ullBytesExtracted);
                        break;
                    case ImportItem::Data:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (data) imported (%I64d bytes)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().ullBytesExtracted);
                        break;
                    case ImportItem::Text:
                        log::Info(
                            _L_,
                            L"\t[%04d] %s (text) imported (%I64d bytes)\r\n",
                            queued,
                            item->Item().Name.c_str(),
                            item->Item().ullBytesExtracted);
                        break;
                }
                ullImportedLines += item->Item().ullLinesImported;
                ullProcessedBytes += item->Item().ullBytesExtracted;

                SystemDetails::WriteComputerName(output);

                if (item->Item().InputFile != nullptr)
                    output.WriteString(item->Item().InputFile->c_str());
                else
                    output.WriteNothing();

                output.WriteString(item->Item().Name.c_str());
                output.WriteString(item->Item().FullName.c_str());

                switch (item->Action())
                {
                    case ImportNotification::Import:
                        output.WriteString(L"Import");
                        break;
                    case ImportNotification::Extract:
                        output.WriteString(L"Extract");
                        break;
                    default:
                        output.WriteString(L"Unknown");
                        break;
                }

                output.WriteString(item->Item().SystemType.c_str());
                output.WriteString(item->Item().ComputerName.c_str());
                output.WriteString(item->Item().TimeStamp.c_str());

                FILETIME start, end;
                SystemTimeToFileTime(&item->Item().ImportStart, &start);
                output.WriteFileTime(start);

                SystemTimeToFileTime(&item->Item().ImportEnd, &end);
                output.WriteFileTime(end);

                switch (item->Action())
                {
                    case ImportNotification::Import:
                        if (item->Item().DefinitionItem)
                            output.WriteString(item->Item().DefinitionItem->Table.c_str());
                        else
                            output.WriteNothing();
                        break;
                    case ImportNotification::Extract:
                        output.WriteNothing();
                        break;
                    default:
                        output.WriteNothing();
                        break;
                }

                switch (item->Action())
                {
                    case ImportNotification::Import:
                        output.WriteNothing();
                        break;
                    case ImportNotification::Extract:
                        if (item->Item().OutputFile)
                        {
                            output.WriteString(item->Item().OutputFile->c_str());
                        }
                        else
                            output.WriteNothing();
                        break;
                    default:
                        output.WriteNothing();
                        break;
                }

                output.WriteInteger(item->Item().ullLinesImported);
                output.WriteInteger(item->Item().ullBytesExtracted);

                output.WriteInteger((DWORD)item->GetHR());

                output.WriteEndOfLine();
            }
            else
            {
                log::Error(_L_, item->GetHR(), L"\t[%04d] %s failed\r\n", queued, item->Item().Name.c_str());

                SystemDetails::WriteComputerName(output);

                if (item->Item().InputFile != nullptr)
                    output.WriteString(item->Item().InputFile->c_str());
                else
                    output.WriteNothing();

                output.WriteString(item->Item().Name.c_str());
                output.WriteString(item->Item().FullName.c_str());

                output.WriteString(L"Import");

                output.WriteNothing();
                output.WriteNothing();
                output.WriteNothing();

                output.WriteNothing();
                output.WriteNothing();

                output.WriteNothing();

                output.WriteNothing();

                output.WriteNothing();
                output.WriteNothing();

                output.WriteInteger((DWORD)item->GetHR());

                output.WriteEndOfLine();
            }
            return;
        });

    if (!config.m_Tables.empty())
    {
        log::Info(_L_, L"\r\nTables configuration :\r\n");
        for (const auto& table : config.m_Tables)
        {
            LPWSTR szDisp = nullptr;
            switch (table.Disposition)
            {
                case AsIs:
                    szDisp = L"as is";
                    break;
                case Truncate:
                    szDisp = L"truncate";
                    break;
                case CreateNew:
                    szDisp = L"create new";
                    break;
                default:
                    break;
            }

            log::Info(
                _L_,
                L"\t%s : %s, %s, %s with %d concurrent agents\r\n",
                table.Name.c_str(),
                szDisp,
                table.bCompress ? L"compression" : L"no compression",
                table.bTABLOCK ? L"table lock" : L"rows lock",
                table.dwConcurrency);
        }
        log::Info(_L_, L"\r\n\r\n");
    }

    m_importAgent =
        std::make_unique<ImportAgent>(_L_, m_importRequestBuffer, m_importRequestBuffer, *m_importNotification);

    if (FAILED(
            hr = m_importAgent->InitializeOutputs(
                config.Output, config.importOutput, config.extractOutput, config.tempOutput)))
    {
        log::Error(_L_, hr, L"Failed to initialize import agent output\r\n");
        return hr;
    }

    if (!config.m_Tables.empty())
    {
        log::Info(_L_, L"\r\nInitialize tables...");
        if (FAILED(hr = m_importAgent->InitializeTables(config.m_Tables)))
        {
            log::Error(_L_, hr, L"\r\nFailed to initialize import agent table definitions\r\n");
            return hr;
        }
        log::Info(_L_, L" Done\r\n");
    }

    log::Info(_L_, L"\r\nEnumerate input files tables...");
    ULONG ulInputFiles = 0L;

    vector<vector<ImportItem>> inputs;
    inputs.reserve(config.Inputs.size());

    for (auto& input : config.Inputs)
    {
        inputs.push_back(GetInputFiles(input));
    }

    auto it = std::max_element(
        begin(inputs), end(inputs), [](const vector<ImportItem>& one, const vector<ImportItem>& two) -> bool {
            return one.size() < two.size();
        });

    auto dwMax = it->size();

    for (unsigned int i = 0; i < dwMax; i++)
    {
        for (auto& input : inputs)
        {
            if (input.size() > i)
            {
                m_importAgent->SendRequest(ImportMessage::MakeExpandRequest(move(input[i])));
                ulInputFiles++;
            }
        }
    }

    log::Info(_L_, L" Done (%d input files)\r\n\r\n", ulInputFiles);

    log::Info(_L_, L"\r\nImporting data...\r\n");

    if (!m_importAgent->start())
    {
        log::Error(_L_, E_FAIL, L"Start for import Agent failed\r\n");
        return E_FAIL;
    }

    try
    {
        concurrency::agent::wait(m_importAgent.get());
    }
    catch (concurrency::operation_timed_out timeout)
    {
        log::Error(_L_, hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT), L"Timed out while waiting for agent completion");
        return hr;
    }

    if (!config.m_Tables.empty())
    {
        log::Info(_L_, L"\r\nAfter statements\r\n");
        if (FAILED(hr = m_importAgent->FinalizeTables()))
        {
            log::Error(_L_, hr, L"Failed to finalize import tables\r\n");
        }
    }

    log::Info(_L_, L"\r\nSome statistics\r\n)");
    m_importAgent->Statistics(_L_);

    m_importAgent.release();

    if (pWriter)
        pWriter->Close();
    pWriter = nullptr;

    return S_OK;
}
