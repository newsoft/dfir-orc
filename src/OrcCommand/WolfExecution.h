//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//
#pragma once

#include <agents.h>

#include <regex>
#include <chrono>

#include "ArchiveMessage.h"
#include "ArchiveAgent.h"

#include "CommandAgent.h"
#include "CommandMessage.h"

#include "TableOutputWriter.h"

#include "UploadAgent.h"

#include "ConfigFile.h"

#include "UploadMessage.h"

#include "Robustness.h"

#include <boost/logic/tribool.hpp>

#pragma managed(push, off)

namespace Orc {

class LogFileWriter;

namespace Command::Wolf {

class WolfTask;

class WolfExecution
{
private:
    class WOLFExecutionTerminate : public TerminationHandler
    {
    private:
        WolfExecution* m_pExec;

    public:
        WOLFExecutionTerminate(const std::wstring& strKeyword, WolfExecution* pExec)
            : TerminationHandler(strKeyword, ROBUSTNESS_ARCHIVE)
            , m_pExec(pExec) {};
        HRESULT operator()()
        {

            if (m_pExec != nullptr)
            {
                return m_pExec->CompleteArchive(nullptr);
            }

            return S_OK;
        };
    };

public:
    typedef enum _Repeat
    {
        NotSet = 0,
        CreateNew = 1,
        Overwrite = 2,
        Once = 3
    } Repeat;

    class Recipient
    {
    public:
        std::wstring Name;
        std::vector<std::wstring> ArchiveSpec;
        CBinaryBuffer Certificate;
    };

private:
    logger _L_;
    std::wstring m_logFilePath;

    std::wstring m_strKeyword;
    std::wstring m_strCompressionLevel;
    DWORD m_dwConcurrency;

    std::chrono::milliseconds m_CmdTimeOut;
    std::chrono::milliseconds m_ArchiveTimeOut;

    bool m_bUseJournalWhenEncrypting = true;
    bool m_bTeeClearTextOutput = false;

    bool m_bOptional = false;

    boost::tribool m_bChildDebug = boost::indeterminate;

    std::shared_ptr<WOLFExecutionTerminate> m_pTermination = nullptr;

    ArchiveMessage::UnboundedMessageBuffer m_ArchiveMessageBuffer;
    std::unique_ptr<Concurrency::call<ArchiveNotification::Notification>> m_archiveNotification;
    std::unique_ptr<ArchiveAgent> m_archiveAgent;
    std::wstring m_strArchiveName;
    OutputSpec m_Output;

    std::wstring m_strOutputFullPath;
    std::wstring m_strOutputFileName;
    std::wstring m_strArchiveFullPath;
    std::wstring m_strArchiveFileName;
    Repeat m_RepeatBehavior = NotSet;

    OutputSpec m_Temporary;

    OutputSpec m_ProcessStatisticsOutput;
    std::shared_ptr<TableOutput::IWriter> m_ProcessStatisticsWriter;

    FILETIME m_StartTime;
    FILETIME m_FinishTime;

    std::shared_ptr<TableOutput::IWriter> m_JobStatisticsWriter;
    OutputSpec m_JobStatisticsOutput;

    std::vector<CommandMessage::Message> m_Commands;

    std::map<std::wstring, std::shared_ptr<WolfTask>> m_TasksByKeyword;
    std::map<DWORD, std::shared_ptr<WolfTask>> m_TasksByPID;
    DWORD m_dwLongerTaskKeyword = 0L;

    CommandMessage::PriorityMessageBuffer m_cmdAgentBuffer;
    std::unique_ptr<Concurrency::call<CommandNotification::Notification>> m_cmdNotification;
    std::unique_ptr<CommandAgent> m_cmdAgent;

    JobRestrictions m_Restrictions;
    std::chrono::milliseconds m_ElapsedTime;  // in millisecs

    std::unique_ptr<Concurrency::timer<CommandMessage::Message>> m_RefreshTimer;
    std::unique_ptr<Concurrency::timer<CommandMessage::Message>> m_KillerTimer;

    std::vector<std::shared_ptr<Recipient>> m_Recipients;

    CommandMessage::Message SetCommandFromConfigItem(const ConfigItem& item);
    HRESULT GetExecutableToRun(const ConfigItem& item, std::wstring& strExeToRun, std::wstring& strArgToAdd);

    HRESULT AddProcessStatistics(ITableOutput& output, const CommandNotification::Notification& notification);
    HRESULT AddJobStatistics(ITableOutput& output, const CommandNotification::Notification& notification);

    HRESULT NotifyTask(const CommandNotification::Notification& item);

    static std::wregex g_WinVerRegEx;

    std::shared_ptr<ByteStream> m_configStream;
    std::shared_ptr<ByteStream> m_localConfigStream;

public:
    WolfExecution(logger pLog)
        : _L_(std::move(pLog)) {};

    HRESULT SetArchiveName(const std::wstring& strArchiveName)
    {
        if (strArchiveName.empty())
            return E_INVALIDARG;
        m_strArchiveName = strArchiveName;
        return S_OK;
    };

    HRESULT SetOutput(
        const OutputSpec& output,
        const OutputSpec& temporary,
        const OutputSpec& jobstats,
        const OutputSpec& processstats)
    {
        if (output.Type != OutputSpec::Kind::Directory)
            return E_INVALIDARG;
        m_Output = output;
        if (temporary.Type != OutputSpec::Kind::Directory)
            return E_INVALIDARG;
        m_Temporary = temporary;

        m_JobStatisticsOutput = jobstats;
        if (m_JobStatisticsOutput.Type & OutputSpec::Kind::TableFile)
        {
            m_JobStatisticsOutput.Path = m_Temporary.Path + L"\\" + m_JobStatisticsOutput.Path;
        }
        m_ProcessStatisticsOutput = processstats;
        if (m_ProcessStatisticsOutput.Type & OutputSpec::Kind::TableFile)
        {
            m_ProcessStatisticsOutput.Path = m_Temporary.Path + L"\\" + m_ProcessStatisticsOutput.Path;
        }
        return S_OK;
    };

    HRESULT SetConfigStreams(const std::shared_ptr<ByteStream>& stream, const std::shared_ptr<ByteStream>& localstream)
    {
        if (stream != nullptr)
        {
            stream->SetFilePointer(0LL, FILE_BEGIN, NULL);
            m_configStream = stream;
        }
        if (localstream != nullptr)
        {
            localstream->SetFilePointer(0LL, FILE_BEGIN, NULL);
            m_localConfigStream = localstream;
        }
        return S_OK;
    }

    HRESULT SetRecipients(const std::vector<std::shared_ptr<WolfExecution::Recipient>> recipients);

    HRESULT BuildFullArchiveName();

    HRESULT CreateArchiveAgent();
    HRESULT CreateCommandAgent(boost::tribool bChildDebug, std::chrono::milliseconds msRefresh, DWORD dwMaxTasks);

    bool UseJournalWhenEncrypting() const { return m_bUseJournalWhenEncrypting; };
    void SetUseJournalWhenEncrypting(bool bUseJournalWhenEncrypting)
    {
        m_bUseJournalWhenEncrypting = bUseJournalWhenEncrypting;
    };

    bool TeeClearTextOutput() const { return m_bTeeClearTextOutput; };
    void SetTeeClearTextOutput(bool bTeeClearTextOutput) { m_bTeeClearTextOutput = bTeeClearTextOutput; };

    HRESULT SetJobTimeOutFromConfig(
        const ConfigItem& item,
        std::chrono::milliseconds dwCmdTimeOut,
        std::chrono::milliseconds dwArchiveTimeOut);
    HRESULT SetJobConfigFromConfig(const ConfigItem& item);
    HRESULT SetCommandsFromConfig(const ConfigItem& item);
    HRESULT SetRestrictionsFromConfig(const ConfigItem& item);
    HRESULT SetRepeatBehaviourFromConfig(const ConfigItem& item);
    HRESULT SetRepeatBehaviour(const Repeat behavior);
    HRESULT SetCompressionLevel(const std::wstring& strCompressionLevel);

    void SetOptional() { m_bOptional = true; }
    void SetMandatory() { m_bOptional = false; }
    bool IsOptional() { return m_bOptional; }

    void SetChildDebug() { m_bChildDebug = true; }
    void UnSetChildDebug() { m_bChildDebug = false; }

    bool IsChildDebugActive(boost::tribool bGlobalSetting) const
    {

        if (boost::indeterminate(bGlobalSetting))
        {
            return (bool)m_bChildDebug;
        }
        else
        {
            return (bool)bGlobalSetting;
        }
    }

    Repeat RepeatBehaviour() const { return m_RepeatBehavior; };

    const std::wstring& GetKeyword() const { return m_strKeyword; };
    DWORD GetConcurrency() const { return m_dwConcurrency; };
    const std::wstring& GetFullArchiveName() const { return m_strArchiveFullPath; };
    const std::wstring& GetArchiveFileName() const { return m_strArchiveFileName; };
    const std::wstring& GetOutputFullPath() const { return m_strOutputFullPath; };
    const std::wstring& GetOutputFileName() const { return m_strOutputFileName; };
    const std::vector<CommandMessage::Message>& GetCommands() const { return m_Commands; };

    std::vector<std::shared_ptr<Recipient>>& Recipients() { return m_Recipients; };
    const std::vector<std::shared_ptr<Recipient>>& Recipients() const { return m_Recipients; };

    HRESULT EnqueueCommands();

    HRESULT CompleteExecution();
    HRESULT TerminateAllAndComplete();

    HRESULT CompleteArchive(UploadMessage::ITarget* pUploadMessageQueue);

    ~WolfExecution(void);
};
}  // namespace Command::Wolf
}  // namespace Orc

#pragma managed(pop)
