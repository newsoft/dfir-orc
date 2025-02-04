//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//
#pragma once

#include "OrcLib.h"

#include "BinaryBuffer.h"

#include <string>
#include <memory>

#include <agents.h>

#pragma managed(push, off)

namespace Orc {

class ArchiveNotification
{
    friend class std::shared_ptr<ArchiveNotification>;
    friend class std::_Ref_count_obj<ArchiveNotification>;

public:
    enum Status
    {
        Success,
        Failure
    };

    enum Type
    {
        Started,
        ArchiveStarted,
        FileAddition,
        DirectoryAddition,
        StreamAddition,
        FlushQueue,
        Cancelled,
        ArchiveComplete
    };

    using Notification = std::shared_ptr<ArchiveNotification>;
    using UnboundedMessageBuffer = Concurrency::unbounded_buffer<Notification>;

    using ITarget = Concurrency::ITarget<Notification>;
    using ISource = Concurrency::ISource<Notification>;

private:
    Status m_Status;
    Type m_Type;
    std::wstring m_keyword;
    std::wstring m_strFileName;
    std::wstring m_description;
    std::wstring m_strCompressionLevel;
    HRESULT m_hr;

    CBinaryBuffer m_md5;
    CBinaryBuffer m_sha1;

protected:
    ArchiveNotification(Type type, Status status, const std::wstring& keyword, const std::wstring& descr, HRESULT hr)
        : m_Type(type)
        , m_Status(status)
        , m_keyword(keyword)
        , m_description(descr)
        , m_hr(hr)
    {
    }

public:
    static Notification MakeSuccessNotification(Type type, const std::wstring& keyword)
    {
        return std::make_shared<ArchiveNotification>(type, Success, keyword, L"", S_OK);
    }
    static Notification
    MakeFailureNotification(Type type, HRESULT hr, const std::wstring& keyword, const std::wstring& description)
    {
        return std::make_shared<ArchiveNotification>(type, Failure, keyword, description, hr);
    }

    static Notification MakeArchiveStartedSuccessNotification(
        const std::wstring& keyword,
        const std::wstring& strFileName,
        const std::wstring& strCompressionLevel)
    {
        auto retval =
            std::make_shared<ArchiveNotification>(ArchiveStarted, Success, keyword, L"Archive creation started", S_OK);

        retval->m_strFileName = strFileName;
        retval->m_strCompressionLevel = strCompressionLevel;
        return retval;
    }

    static Notification
    MakeAddFileSucessNotification(const std::wstring& keyword, CBinaryBuffer& md5, CBinaryBuffer& sha1)
    {
        auto retval = std::make_shared<ArchiveNotification>(FileAddition, Success, keyword, L"", S_OK);
        retval->m_md5 = md5;
        retval->m_sha1 = sha1;
        return retval;
    }
    static Notification
    MakeAddFileSucessNotification(const std::wstring keyword, CBinaryBuffer&& md5, CBinaryBuffer&& sha1)
    {
        auto retval = std::make_shared<ArchiveNotification>(FileAddition, Success, keyword, L"", S_OK);
        std::swap(retval->m_md5, md5);
        std::swap(retval->m_sha1, sha1);
        return retval;
    }

    Status GetStatus() const { return m_Status; };
    HRESULT GetHResult() const { return m_hr; };
    Type GetType() const { return m_Type; };

    const std::wstring& Keyword() const { return m_keyword; };
    const std::wstring& Description() const { return m_description; }
    const std::wstring& GetFileName() const { return m_strFileName; }
    const std::wstring& GetCompressionLevel() const { return m_strCompressionLevel; }

    const CBinaryBuffer& MD5() const { return m_md5; };
    const CBinaryBuffer& SHA1() const { return m_sha1; };

    ~ArchiveNotification(void);
};

}  // namespace Orc

#pragma managed(pop)
