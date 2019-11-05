/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Stub TLS Implementation for QUIC

--*/

#include "platform_internal.h"

#ifdef QUIC_LOGS_WPP
#include "tls_stub.tmh"
#endif

uint16_t QuicTlsTPHeaderSize = 0;

#define TLS1_PROTOCOL_VERSION 0x0301
#define TLS_MESSAGE_HEADER_LENGTH 4
#define TLS_RANDOM_LENGTH 32
#define TLS_SESSION_ID_LENGTH 32

typedef enum eTlsHandshakeType {
    TlsHandshake_ClientHello = 0x01
} eTlsHandshakeType;

typedef enum eTlsExtensions {
    TlsExt_ServerName = 0x00,
    TlsExt_AppProtocolNegotiation = 0x10,
    TlsExt_SessionTicket = 0x23,
    TlsExt_QuicTransportParameters = 0xffa5,
} eTlsExtensions;

typedef enum eSniNameType {
    TlsExt_Sni_NameType_HostName = 0
} eSniNameType;

#define MAX_PARAM_LENGTH 256

typedef enum _QUIC_FAKE_TLS_MESSAGE_TYPE {

    QUIC_TLS_MESSAGE_INVALID,
    QUIC_TLS_MESSAGE_CLIENT_INITIAL,
    QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE,
    QUIC_TLS_MESSAGE_SERVER_INITIAL,
    QUIC_TLS_MESSAGE_SERVER_HANDSHAKE,
    QUIC_TLS_MESSAGE_TICKET,
    QUIC_TLS_MESSAGE_MAX

} QUIC_FAKE_TLS_MESSAGE_TYPE;

QUIC_STATIC_ASSERT(
    (uint32_t)QUIC_TLS_MESSAGE_CLIENT_INITIAL == (uint32_t)TlsHandshake_ClientHello,
    "Stub need to fake client hello exactly");

const uint16_t MinMessageLengths[] = {
    0,                              // QUIC_TLS_MESSAGE_INVALID
    0,                              // QUIC_TLS_MESSAGE_CLIENT_INITIAL (Dynamic)
    7 + 1,                          // QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE
    7 + 1,                          // QUIC_TLS_MESSAGE_SERVER_INITIAL
    7 + 3 + MAX_PARAM_LENGTH,       // QUIC_TLS_MESSAGE_SERVER_HANDSHAKE
    7 + 1                           // QUIC_TLS_MESSAGE_TICKET
};

static
uint16_t
TlsReadUint16(
    _In_reads_(2) const uint8_t* Buffer
    )
{
    return
        (((uint32_t)Buffer[0] << 8) +
          (uint32_t)Buffer[1]);
}

static
void
TlsWriteUint16(
    _Out_writes_all_(2) uint8_t* Buffer,
    _In_ uint16_t Value
    )
{
    Buffer[0] = (uint8_t)(Value >> 8);
    Buffer[1] = (uint8_t)Value;
}

static
uint32_t
TlsReadUint24(
    _In_reads_(3) const uint8_t* Buffer
    )
{
    return
        (((uint32_t)Buffer[0] << 16) +
         ((uint32_t)Buffer[1] << 8) +
          (uint32_t)Buffer[2]);
}

static
void
TlsWriteUint24(
    _Out_writes_all_(3) uint8_t* Buffer,
    _In_ uint32_t Value
    )
{
    Buffer[0] = (uint8_t)(Value >> 16);
    Buffer[1] = (uint8_t)(Value >> 8);
    Buffer[2] = (uint8_t)Value;
}

#pragma pack(push)
#pragma pack(1)

typedef struct _QUIC_TLS_SNI_EXT {
    uint8_t ExtType[2];                 // TlsExt_ServerName
    uint8_t ExtLen[2];
    uint8_t ListLen[2];
    uint8_t NameType;                   // TlsExt_Sni_NameType_HostName
    uint8_t NameLength[2];
    uint8_t Name[0];
} QUIC_TLS_SNI_EXT;

typedef struct _QUIC_TLS_ALPN_EXT {
    uint8_t ExtType[2];                 // TlsExt_AppProtocolNegotiation
    uint8_t ExtLen[2];
    uint8_t AlpnListLength[2];
    uint8_t AlpnLength;
    uint8_t Alpn[0];
} QUIC_TLS_ALPN_EXT;

typedef struct _QUIC_TLS_SESSION_TICKET_EXT {
    uint8_t ExtType[2];                 // TlsExt_SessionTicket
    uint8_t ExtLen[2];
    uint8_t Ticket[0];
} QUIC_TLS_SESSION_TICKET_EXT;

typedef struct _QUIC_TLS_QUIC_TP_EXT {
    uint8_t ExtType[2];                 // TlsExt_QuicTransportParameters
    uint8_t ExtLen[2];
    uint8_t TP[0];
} QUIC_TLS_QUIC_TP_EXT;

typedef struct _QUIC_TLS_CLIENT_HELLO { // All multi-byte fields are Network Byte Order
    uint8_t Version[2];
    uint8_t Random[TLS_RANDOM_LENGTH];
    uint8_t SessionIdLength;            // 0
    uint8_t CipherSuiteLength[2];
    uint8_t CompressionMethodLength;    // 1
    uint8_t CompressionMethod;

    uint8_t ExtListLength[2];
    uint8_t ExtList[0];
    // QUIC_TLS_SNI_EXT
    // QUIC_TLS_ALPN_EXT
    // QUIC_TLS_SESSION_TICKET_EXT
    // QUIC_TLS_QUIC_TP_EXT
} QUIC_TLS_CLIENT_HELLO;

typedef struct _QUIC_FAKE_TLS_MESSAGE {
    uint8_t Type;
    uint8_t Length[3]; // Uses TLS 24-bit length encoding
    union {
        QUIC_TLS_CLIENT_HELLO CLIENT_INITIAL;
        struct {
            uint8_t Success;
        } CLIENT_HANDSHAKE;
        struct {
            uint8_t Success : 1;
            uint8_t EarlyDataAccepted : 1;
        } SERVER_INITIAL;
        struct {
            uint8_t QuicTPLength;
            uint8_t QuicTP[MAX_PARAM_LENGTH];
            uint16_t CertificateLength;
            uint8_t Certificate[0];
        } SERVER_HANDSHAKE;
        struct {
            uint8_t HasTicket;
        } TICKET;
    };
} QUIC_FAKE_TLS_MESSAGE, *PQUIC_FAKE_TLS_MESSAGE;

#pragma pack(pop)

typedef struct _QUIC_TLS_SESSION {

    uint16_t AlpnLength;
    const char* Alpn[0];

} QUIC_TLS_SESSION, *PQUIC_TLS_SESSION;

typedef struct _QUIC_SEC_CONFIG {

    QUIC_RUNDOWN_REF* CleanupRundown;
    long RefCount;
    uint32_t Flags;
    PQUIC_CERT Certificate;
    uint16_t FormatLength;
    uint8_t FormatBuffer[SIZEOF_CERT_CHAIN_LIST_LENGTH];

} QUIC_SEC_CONFIG;

typedef struct _QUIC_TLS {

    BOOLEAN IsServer : 1;
    //BOOLEAN EarlyDataConfigured : 1;
    //BOOLEAN EarlyDataAccepted : 1;
    BOOLEAN TicketReady : 1;

    QUIC_FAKE_TLS_MESSAGE_TYPE LastMessageType; // Last message sent.

    PQUIC_TLS_SESSION TlsSession;
    QUIC_SEC_CONFIG* SecConfig;

    PQUIC_CONNECTION Connection;
    QUIC_TLS_RECEIVE_TP_CALLBACK_HANDLER ReceiveTPCallback;

    const char* SNI;

    const uint8_t* LocalTPBuffer;
    uint32_t LocalTPLength;

} QUIC_TLS, *PQUIC_TLS;

char
GetTlsIdentifier(
    _In_ const QUIC_TLS* TlsContext
    )
{
    const char IDs[2] = { 'C', 'S' };
    return IDs[TlsContext->IsServer];
}

__drv_allocatesMem(Mem)
QUIC_PACKET_KEY*
QuicStubAllocKey(
    _In_ QUIC_PACKET_KEY_TYPE Type
    )
{
    size_t PacketKeySize = 
        sizeof(QUIC_PACKET_KEY) +
        (Type == QUIC_PACKET_KEY_1_RTT ? sizeof(QUIC_SECRET) : 0);
    QUIC_PACKET_KEY *Key = QUIC_ALLOC_NONPAGED(PacketKeySize);
    QUIC_DBG_ASSERT(Key != NULL);
    QuicZeroMemory(Key, PacketKeySize);
    Key->Type = Type;
    return Key;
}

QUIC_STATUS
QuicTlsLibraryInitialize(
    void
    )
{
    return QUIC_STATUS_SUCCESS;
}

void
QuicTlsLibraryUninitialize(
    void
    )
{
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsServerSecConfigCreate(
    _Inout_ QUIC_RUNDOWN_REF* Rundown,
    _In_ QUIC_SEC_CONFIG_FLAGS Flags,
    _In_opt_ void* Certificate,
    _In_opt_z_ const char* Principal,
    _In_opt_ void* Context,
    _In_ QUIC_SEC_CONFIG_CREATE_COMPLETE_HANDLER CompletionHandler
    )
{
    QUIC_STATUS Status;
    QUIC_SEC_CONFIG* SecurityConfig = NULL;

    if (!QuicRundownAcquire(Rundown)) {
        LogError("[ tls] Failed to acquire sec config rundown.");
        Status = QUIC_STATUS_INVALID_STATE;
        goto Error;
    }

#pragma prefast(suppress: __WARNING_6014, "Memory is correctly freed (QuicTlsSecConfigDelete).")
    SecurityConfig = QUIC_ALLOC_PAGED(sizeof(QUIC_SEC_CONFIG));
    if (SecurityConfig == NULL) {
        QuicRundownRelease(Rundown);
        Status = QUIC_STATUS_OUT_OF_MEMORY;
        goto Error;
    }

    SecurityConfig->CleanupRundown = Rundown;
    SecurityConfig->RefCount = 1;
    SecurityConfig->Flags = Flags;

    if (Flags == QUIC_SEC_CONFIG_FLAG_CERTIFICATE_NULL) {
        //
        // Using NULL certificate.
        //
        goto Format;
    } else if (Flags & QUIC_SEC_CONFIG_FLAG_CERTIFICATE_FILE) {
        Status = QUIC_STATUS_INVALID_PARAMETER;
        goto Error;
    } else if (Flags & QUIC_SEC_CONFIG_FLAG_CERTIFICATE_CONTEXT) {
        if (Certificate == NULL) {
            Status = QUIC_STATUS_INVALID_PARAMETER;
            goto Error;
        }
        SecurityConfig->Certificate = (PQUIC_CERT)Certificate;
    } else {
        Status =
            QuicCertCreate(
                Flags,
                Certificate,
                Principal,
                &SecurityConfig->Certificate);
        if (QUIC_FAILED(Status)) {
            goto Error;
        }
    }

Format:

    SecurityConfig->FormatLength =
        (uint16_t)QuicCertFormat(
            SecurityConfig->Certificate,
            sizeof(SecurityConfig->FormatBuffer),
            SecurityConfig->FormatBuffer);

    Status = QUIC_STATUS_SUCCESS;

    CompletionHandler(
        Context,
        Status,
        SecurityConfig);
    SecurityConfig = NULL;

Error:

    if (SecurityConfig != NULL) {
        QUIC_FREE(SecurityConfig);
        QuicRundownRelease(Rundown);
    }

    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsSecConfigDelete(
    _In_ QUIC_SEC_CONFIG* SecurityConfig
    )
{
    if (!(SecurityConfig->Flags & QUIC_SEC_CONFIG_FLAG_CERTIFICATE_CONTEXT)) {
        QuicCertFree(SecurityConfig->Certificate);
    }
    QUIC_RUNDOWN_REF* Rundown = SecurityConfig->CleanupRundown;
    QUIC_FREE(SecurityConfig);
    if (Rundown != NULL) {
        QuicRundownRelease(Rundown);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsClientSecConfigCreate(
    _In_ uint32_t Flags,
    _Outptr_ QUIC_SEC_CONFIG** ClientConfig
    )
{
    #pragma prefast(suppress: __WARNING_6014, "Memory is correctly freed (QuicTlsSecConfigDelete).")
    QUIC_SEC_CONFIG* SecurityConfig = QUIC_ALLOC_PAGED(sizeof(QUIC_SEC_CONFIG));
    if (SecurityConfig == NULL) {
        return QUIC_STATUS_OUT_OF_MEMORY;
    }

    QuicZeroMemory(SecurityConfig, sizeof(*SecurityConfig));
    SecurityConfig->Flags = (uint32_t)Flags;
    SecurityConfig->RefCount = 1;

    *ClientConfig = SecurityConfig;

    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
inline
QUIC_SEC_CONFIG*
QuicTlsSecConfigAddRef(
    _In_ QUIC_SEC_CONFIG* SecurityConfig
    )
{
    InterlockedIncrement(&SecurityConfig->RefCount);
    return SecurityConfig;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QUIC_API
QuicTlsSecConfigRelease(
    _In_ QUIC_SEC_CONFIG* SecurityConfig
    )
{
    if (InterlockedDecrement(&SecurityConfig->RefCount) == 0) {
        QuicTlsSecConfigDelete(SecurityConfig);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsSessionInitialize(
    _In_z_ const char* ALPN,
    _Out_ PQUIC_TLS_SESSION* NewTlsSession
    )
{
    QUIC_STATUS Status;
    PQUIC_TLS_SESSION TlsSession = NULL;

    size_t ALPNLength = strlen(ALPN);
    if (ALPNLength > UINT16_MAX) {
        Status = QUIC_STATUS_INVALID_PARAMETER;
        goto Error;
    }

    TlsSession = QUIC_ALLOC_PAGED(sizeof(QUIC_TLS_SESSION) + ALPNLength);
    if (TlsSession == NULL) {
        LogWarning("[ tls] Failed to allocate QUIC_TLS_SESSION.");
        Status = QUIC_STATUS_OUT_OF_MEMORY;
        goto Error;
    }

    QuicCopyMemory((char*)TlsSession->Alpn, ALPN, ALPNLength);
    TlsSession->AlpnLength = (uint16_t)ALPNLength;

    *NewTlsSession = TlsSession;
    TlsSession = NULL;

    Status = QUIC_STATUS_SUCCESS;

Error:

    if (TlsSession != NULL) {
        QUIC_FREE(TlsSession);
    }

    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsSessionUninitialize(
    _In_opt_ PQUIC_TLS_SESSION TlsSession
    )
{
    if (TlsSession != NULL) {
        QUIC_FREE(TlsSession);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsSessionSetTicketKey(
    _In_ QUIC_TLS_SESSION* TlsSession,
    _In_reads_bytes_(44)
        const void* Buffer
    )
{
    UNREFERENCED_PARAMETER(TlsSession);
    UNREFERENCED_PARAMETER(Buffer);
    //
    // TODO - Use encryption key.
    //
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsSessionAddTicket(
    _In_ PQUIC_TLS_SESSION TlsSession,
    _In_ uint32_t BufferLength,
    _In_reads_bytes_(BufferLength)
        const uint8_t * const Buffer
    )
{
    UNREFERENCED_PARAMETER(TlsSession);
    UNREFERENCED_PARAMETER(BufferLength);
    UNREFERENCED_PARAMETER(Buffer);
    //
    // TODO - Add fake ticket store.
    //
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsInitialize(
    _In_ const QUIC_TLS_CONFIG* Config,
    _Out_ PQUIC_TLS* NewTlsContext
    )
{
    QUIC_STATUS Status;

    PQUIC_TLS TlsContext = QUIC_ALLOC_PAGED(sizeof(QUIC_TLS));
    if (TlsContext == NULL) {
        LogWarning("[ tls] Failed to allocate QUIC_TLS.");
        Status = QUIC_STATUS_OUT_OF_MEMORY;
        goto Exit;
    }

    QuicZeroMemory(TlsContext, sizeof(QUIC_TLS));

    TlsContext->IsServer = Config->IsServer;
    TlsContext->TlsSession = Config->TlsSession;
    TlsContext->LocalTPBuffer = Config->LocalTPBuffer;
    TlsContext->LocalTPLength = Config->LocalTPLength;
    TlsContext->SecConfig = QuicTlsSecConfigAddRef(Config->SecConfig);
    TlsContext->Connection = Config->Connection;
    TlsContext->ReceiveTPCallback = Config->ReceiveTPCallback;

    LogVerbose("[ tls][%p][%c] Created.",
        TlsContext, GetTlsIdentifier(TlsContext));

    if (Config->ServerName != NULL) {
        const size_t ServerNameLength =
            strnlen(Config->ServerName, QUIC_MAX_SNI_LENGTH + 1);
        if (ServerNameLength == QUIC_MAX_SNI_LENGTH + 1) {
            LogError("[ tls][%p][%c] Invalid / Too long server name!",
                TlsContext, GetTlsIdentifier(TlsContext));
            Status = QUIC_STATUS_INVALID_PARAMETER;
            goto Error;
        }

        TlsContext->SNI = QUIC_ALLOC_PAGED(ServerNameLength + 1);
        if (TlsContext->SNI == NULL) {
            LogWarning("[ tls][%p][%c] Failed to allocate SNI.",
                TlsContext, GetTlsIdentifier(TlsContext));
            Status = QUIC_STATUS_OUT_OF_MEMORY;
            goto Error;
        }
        memcpy((char*)TlsContext->SNI, Config->ServerName, ServerNameLength + 1);
    }

    *NewTlsContext = TlsContext;
    Status = QUIC_STATUS_SUCCESS;

Error:

    if (QUIC_FAILED(Status)) {
        QuicTlsSecConfigRelease(TlsContext->SecConfig);
        if (TlsContext->SNI) {
            QUIC_FREE(TlsContext->SNI);
        }
        QUIC_FREE(TlsContext);
    }

Exit:

    return Status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsUninitialize(
    _In_opt_ PQUIC_TLS TlsContext
    )
{
    if (TlsContext != NULL) {
        LogVerbose("[ tls][%p][%c] Cleaning up.",
            TlsContext, GetTlsIdentifier(TlsContext));

        if (TlsContext->SecConfig != NULL) {
            QuicTlsSecConfigRelease(TlsContext->SecConfig);
        }

        if (TlsContext->SNI != NULL) {
            QUIC_FREE(TlsContext->SNI);
        }
        
        if (TlsContext->LocalTPBuffer != NULL) {
            QUIC_FREE(TlsContext->LocalTPBuffer);
        }

        QUIC_FREE(TlsContext);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsReset(
    _In_ PQUIC_TLS TlsContext
    )
{
    LogInfo("[ tls][%p][%c] Resetting TLS state.",
        TlsContext, GetTlsIdentifier(TlsContext));

    QUIC_DBG_ASSERT(TlsContext->IsServer == FALSE);
    TlsContext->LastMessageType = QUIC_TLS_MESSAGE_INVALID;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_SEC_CONFIG*
QuicTlsGetSecConfig(
    _In_ PQUIC_TLS TlsContext
    )
{
    return QuicTlsSecConfigAddRef(TlsContext->SecConfig);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsServerProcess(
    _In_ PQUIC_TLS TlsContext,
    _Out_ QUIC_TLS_RESULT_FLAGS* ResultFlags,
    _Inout_ QUIC_TLS_PROCESS_STATE* State,
    _Inout_ uint32_t * BufferLength,
    _In_reads_bytes_(*BufferLength) const uint8_t * Buffer
    )
{
    uint16_t DrainLength = 0;

    QUIC_DBG_ASSERT(State->BufferLength < State->BufferAllocLength);
    __assume(State->BufferLength < State->BufferAllocLength);

    const QUIC_FAKE_TLS_MESSAGE* ClientMessage =
        (QUIC_FAKE_TLS_MESSAGE*)Buffer;
    QUIC_FAKE_TLS_MESSAGE* ServerMessage =
        (QUIC_FAKE_TLS_MESSAGE*)(State->Buffer + State->BufferLength);
    uint16_t MaxServerMessageLength =
        State->BufferAllocLength - State->BufferLength;

    switch (TlsContext->LastMessageType) {

    case QUIC_TLS_MESSAGE_INVALID: {
        QUIC_FRE_ASSERT(ClientMessage->Type == QUIC_TLS_MESSAGE_CLIENT_INITIAL);

        State->EarlyDataAttempted = FALSE;
        State->EarlyDataAccepted = FALSE;

        const uint8_t* ExtList = ClientMessage->CLIENT_INITIAL.ExtList;
        uint16_t ExtListLength = TlsReadUint16(ClientMessage->CLIENT_INITIAL.ExtListLength);
        while (ExtListLength > 0) {
            uint16_t ExtType = TlsReadUint16(ExtList);
            uint16_t ExtLength = TlsReadUint16(ExtList + 2);
            QUIC_FRE_ASSERT(ExtLength + 4 <= ExtListLength);

            switch (ExtType) {
            case TlsExt_ServerName: {
                const QUIC_TLS_SNI_EXT* SNI = (QUIC_TLS_SNI_EXT*)ExtList;
                uint16_t NameLength = TlsReadUint16(SNI->NameLength);
                if (SNI->NameLength != 0) {
                    TlsContext->SNI = QUIC_ALLOC_PAGED(NameLength + 1);
                    memcpy((char*)TlsContext->SNI, SNI->Name, NameLength);
                    ((char*)TlsContext->SNI)[NameLength] = 0;
                }
                break;
            }
            case TlsExt_AppProtocolNegotiation: {
                break; // Ignored in this code.
            }
            case TlsExt_SessionTicket: {
                State->EarlyDataAttempted = TRUE;
                State->EarlyDataAccepted = TRUE; // TODO - Support tickets
                break;
            }
            case TlsExt_QuicTransportParameters: {
                const QUIC_TLS_QUIC_TP_EXT* QuicTP = (QUIC_TLS_QUIC_TP_EXT*)ExtList;
                TlsContext->ReceiveTPCallback(
                    TlsContext->Connection,
                    ExtLength,
                    QuicTP->TP);
                break;
            }
            default:
                QUIC_FRE_ASSERT(FALSE);
                break;
            }

            ExtList += ExtLength + 4;
            ExtListLength -= ExtLength + 4;
        }

        const QUIC_SEC_CONFIG* SecurityConfig = TlsContext->SecConfig;
        QUIC_DBG_ASSERT(SecurityConfig != NULL);

        if (MaxServerMessageLength < MinMessageLengths[QUIC_TLS_MESSAGE_SERVER_INITIAL]) {
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        const uint16_t SignAlgo = 0x0804;
        uint16_t SelectedSignAlgo;

        if (!QuicCertSelect(
                SecurityConfig->Certificate,
                &SignAlgo,
                1,
                &SelectedSignAlgo)) {
            LogError("[ tls][%p][%c] No matching signature algorithm for the provided server certificate.",
                TlsContext, GetTlsIdentifier(TlsContext));
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        uint16_t MessageLength = MinMessageLengths[QUIC_TLS_MESSAGE_SERVER_INITIAL];
        TlsWriteUint24(ServerMessage->Length, MessageLength - 4);
        ServerMessage->Type = QUIC_TLS_MESSAGE_SERVER_INITIAL;
        ServerMessage->SERVER_INITIAL.EarlyDataAccepted = State->EarlyDataAccepted;
        State->EarlyDataAttempted = State->EarlyDataAttempted;
        State->EarlyDataAccepted = State->EarlyDataAccepted;

        State->BufferLength = MessageLength;
        State->BufferTotalLength = MessageLength;
        State->BufferOffsetHandshake = State->BufferTotalLength;

        ServerMessage =
            (QUIC_FAKE_TLS_MESSAGE*)(State->Buffer + State->BufferLength);
        MaxServerMessageLength =
            State->BufferAllocLength - State->BufferLength;

        if (MaxServerMessageLength < MinMessageLengths[QUIC_TLS_MESSAGE_SERVER_HANDSHAKE] + SecurityConfig->FormatLength) {
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        if (State->EarlyDataAccepted) {
            *ResultFlags |= QUIC_TLS_RESULT_EARLY_DATA_ACCEPT;
            State->ReadKeys[QUIC_PACKET_KEY_0_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_0_RTT);
        }

        *ResultFlags |= QUIC_TLS_RESULT_READ_KEY_UPDATED;
        State->ReadKey = QUIC_PACKET_KEY_HANDSHAKE;
        State->ReadKeys[QUIC_PACKET_KEY_HANDSHAKE] = QuicStubAllocKey(QUIC_PACKET_KEY_HANDSHAKE);

        *ResultFlags |= QUIC_TLS_RESULT_WRITE_KEY_UPDATED;
        State->WriteKey = QUIC_PACKET_KEY_HANDSHAKE;
        State->WriteKeys[QUIC_PACKET_KEY_HANDSHAKE] = QuicStubAllocKey(QUIC_PACKET_KEY_HANDSHAKE);

        MessageLength = MinMessageLengths[QUIC_TLS_MESSAGE_SERVER_HANDSHAKE] + SecurityConfig->FormatLength;
        TlsWriteUint24(ServerMessage->Length, MessageLength - 4);
        ServerMessage->Type = QUIC_TLS_MESSAGE_SERVER_HANDSHAKE;
        ServerMessage->SERVER_HANDSHAKE.QuicTPLength = (uint8_t)TlsContext->LocalTPLength;
        memcpy(ServerMessage->SERVER_HANDSHAKE.QuicTP, TlsContext->LocalTPBuffer, TlsContext->LocalTPLength);
        ServerMessage->SERVER_HANDSHAKE.CertificateLength = SecurityConfig->FormatLength;
        memcpy(ServerMessage->SERVER_HANDSHAKE.Certificate, SecurityConfig->FormatBuffer, SecurityConfig->FormatLength);

        State->BufferLength += MessageLength;
        State->BufferTotalLength += MessageLength;
        State->BufferOffset1Rtt = State->BufferTotalLength;
        *ResultFlags |= QUIC_TLS_RESULT_DATA;

        *ResultFlags |= QUIC_TLS_RESULT_WRITE_KEY_UPDATED;
        State->WriteKey = QUIC_PACKET_KEY_1_RTT;
        State->WriteKeys[QUIC_PACKET_KEY_1_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_1_RTT);

        DrainLength = (uint16_t)TlsReadUint24(ClientMessage->Length) + 4;

        TlsContext->LastMessageType = QUIC_TLS_MESSAGE_SERVER_HANDSHAKE;
        break;
    }

    case QUIC_TLS_MESSAGE_SERVER_HANDSHAKE: {
        if (ClientMessage->Type == QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE) {

            if (ClientMessage->CLIENT_HANDSHAKE.Success == FALSE) {
                LogError("[ tls][%p][%c] Failure client finish.",
                    TlsContext, GetTlsIdentifier(TlsContext));
                *ResultFlags |= QUIC_TLS_RESULT_ERROR;
                break;
            }

            *ResultFlags |= QUIC_TLS_RESULT_COMPLETE;

            LogInfo("[ tls][%p][%c] Handshake complete.",
                TlsContext, GetTlsIdentifier(TlsContext));

            QuicTlsSecConfigRelease(TlsContext->SecConfig);
            TlsContext->SecConfig = NULL;

            if (MaxServerMessageLength < MinMessageLengths[QUIC_TLS_MESSAGE_TICKET]) {
                *ResultFlags |= QUIC_TLS_RESULT_ERROR;
                break;
            }

            uint16_t MessageLength = MinMessageLengths[QUIC_TLS_MESSAGE_TICKET];
            TlsWriteUint24(ServerMessage->Length, MessageLength - 4);
            ServerMessage->Type = QUIC_TLS_MESSAGE_TICKET;
            ServerMessage->TICKET.HasTicket = TRUE;

            *ResultFlags |= QUIC_TLS_RESULT_DATA;
            State->BufferLength += MessageLength;
            State->BufferTotalLength += MessageLength;

            *ResultFlags |= QUIC_TLS_RESULT_READ_KEY_UPDATED;
            State->ReadKey = QUIC_PACKET_KEY_1_RTT;
            State->ReadKeys[QUIC_PACKET_KEY_1_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_1_RTT);

            TlsContext->LastMessageType = QUIC_TLS_MESSAGE_TICKET;

        } else {
            LogError("[ tls][%p][%c] Invalid message, %u.",
                TlsContext, GetTlsIdentifier(TlsContext), ClientMessage->Type);
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        DrainLength = (uint16_t)TlsReadUint24(ClientMessage->Length) + 4;

        break;
    }

    default: {
        LogError("[ tls][%p][%c] Invalid last message, %u.",
            TlsContext, GetTlsIdentifier(TlsContext), TlsContext->LastMessageType);
        *ResultFlags |= QUIC_TLS_RESULT_ERROR;
        break;
    }
    }

    *BufferLength = DrainLength;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
QuicTlsClientProcess(
    _In_ PQUIC_TLS TlsContext,
    _Out_ QUIC_TLS_RESULT_FLAGS* ResultFlags,
    _Inout_ QUIC_TLS_PROCESS_STATE* State,
    _Inout_ uint32_t* BufferLength,
    _In_reads_bytes_(*BufferLength) const uint8_t * Buffer
    )
{
    uint16_t DrainLength = 0;

    QUIC_DBG_ASSERT(State->BufferLength < State->BufferAllocLength);
    __assume(State->BufferLength < State->BufferAllocLength);

    const QUIC_FAKE_TLS_MESSAGE* ServerMessage =
        (QUIC_FAKE_TLS_MESSAGE*)Buffer;
    QUIC_FAKE_TLS_MESSAGE* ClientMessage =
        (QUIC_FAKE_TLS_MESSAGE*)(State->Buffer + State->BufferLength);
    uint16_t MaxClientMessageLength =
        State->BufferAllocLength - State->BufferLength;

    switch (TlsContext->LastMessageType) {

    case QUIC_TLS_MESSAGE_INVALID: {

        State->EarlyDataAttempted = TRUE; // TODO - Fake Ticket Store.
        State->EarlyDataAccepted = FALSE; // Default to FALSE.

        ClientMessage->Type = TlsHandshake_ClientHello;

        TlsWriteUint16(ClientMessage->CLIENT_INITIAL.Version, 0x0302);
        ClientMessage->CLIENT_INITIAL.SessionIdLength = 0;
        TlsWriteUint16(ClientMessage->CLIENT_INITIAL.CipherSuiteLength, 0);
        ClientMessage->CLIENT_INITIAL.CompressionMethodLength = 1;

        uint16_t ExtListLength = 0;

        if (TlsContext->SNI != NULL) {
            QUIC_TLS_SNI_EXT* SNI = (QUIC_TLS_SNI_EXT*)ClientMessage->CLIENT_INITIAL.ExtList;
            uint16_t SniNameLength = (uint16_t)strlen(TlsContext->SNI);
            TlsWriteUint16(SNI->ExtType, TlsExt_ServerName);
            TlsWriteUint16(SNI->ExtLen, 5 + SniNameLength);
            TlsWriteUint16(SNI->ListLen, 3 + SniNameLength);
            SNI->NameType = TlsExt_Sni_NameType_HostName;
            TlsWriteUint16(SNI->NameLength, SniNameLength);
            memcpy(SNI->Name, TlsContext->SNI, SniNameLength);
            ExtListLength += 9 + SniNameLength;
        }

        QUIC_TLS_ALPN_EXT* ALPN =
            (QUIC_TLS_ALPN_EXT*)
            (ClientMessage->CLIENT_INITIAL.ExtList + ExtListLength);
        uint16_t AlpnLength = TlsContext->TlsSession->AlpnLength;
        TlsWriteUint16(ALPN->ExtType, TlsExt_AppProtocolNegotiation);
        TlsWriteUint16(ALPN->ExtLen, 3 + AlpnLength);
        TlsWriteUint16(ALPN->AlpnListLength, 1 + AlpnLength);
        ALPN->AlpnLength = (uint8_t)AlpnLength;
        memcpy(ALPN->Alpn, TlsContext->TlsSession->Alpn, AlpnLength);
        ExtListLength += 7 + AlpnLength;

        if (State->EarlyDataAttempted) {
            QUIC_TLS_SESSION_TICKET_EXT* Ticket =
                (QUIC_TLS_SESSION_TICKET_EXT*)
                (ClientMessage->CLIENT_INITIAL.ExtList + ExtListLength);
            TlsWriteUint16(Ticket->ExtType, TlsExt_SessionTicket);
            TlsWriteUint16(Ticket->ExtLen, 0);
            ExtListLength += 4;
        }

        QUIC_TLS_QUIC_TP_EXT* QuicTP =
            (QUIC_TLS_QUIC_TP_EXT*)
            (ClientMessage->CLIENT_INITIAL.ExtList + ExtListLength);
        TlsWriteUint16(QuicTP->ExtType, TlsExt_QuicTransportParameters);
        TlsWriteUint16(QuicTP->ExtLen, (uint16_t)TlsContext->LocalTPLength);
        memcpy(QuicTP->TP, TlsContext->LocalTPBuffer, TlsContext->LocalTPLength);
        ExtListLength += 4 + (uint16_t)TlsContext->LocalTPLength;

        TlsWriteUint16(ClientMessage->CLIENT_INITIAL.ExtListLength, ExtListLength);

        uint16_t MessageLength = sizeof(QUIC_TLS_CLIENT_HELLO) + ExtListLength + 4;
        TlsWriteUint24(ClientMessage->Length, MessageLength - 4);

        *ResultFlags |= QUIC_TLS_RESULT_DATA;
        State->BufferLength = MessageLength;
        State->BufferTotalLength = MessageLength;

        if (State->EarlyDataAttempted) {
            State->WriteKey = QUIC_PACKET_KEY_0_RTT;
            State->WriteKeys[QUIC_PACKET_KEY_0_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_0_RTT);
        }

        TlsContext->LastMessageType = QUIC_TLS_MESSAGE_CLIENT_INITIAL;
        break;
    }

    case QUIC_TLS_MESSAGE_CLIENT_INITIAL: {
        if (ServerMessage->Type == QUIC_TLS_MESSAGE_SERVER_INITIAL) {

            if (State->EarlyDataAttempted) {
                State->EarlyDataAccepted = ServerMessage->SERVER_INITIAL.EarlyDataAccepted;
                if (!ServerMessage->SERVER_INITIAL.EarlyDataAccepted) {
                    *ResultFlags |= QUIC_TLS_RESULT_EARLY_DATA_REJECT;
                } else {
                    *ResultFlags |= QUIC_TLS_RESULT_EARLY_DATA_ACCEPT;
                }
            }

            State->BufferOffsetHandshake = State->BufferTotalLength;

            *ResultFlags |= QUIC_TLS_RESULT_READ_KEY_UPDATED;
            State->ReadKey = QUIC_PACKET_KEY_HANDSHAKE;
            State->ReadKeys[QUIC_PACKET_KEY_HANDSHAKE] = QuicStubAllocKey(QUIC_PACKET_KEY_HANDSHAKE);

            *ResultFlags |= QUIC_TLS_RESULT_WRITE_KEY_UPDATED;
            State->WriteKey = QUIC_PACKET_KEY_HANDSHAKE;
            State->WriteKeys[QUIC_PACKET_KEY_HANDSHAKE] = QuicStubAllocKey(QUIC_PACKET_KEY_HANDSHAKE);

        } else if (ServerMessage->Type == QUIC_TLS_MESSAGE_SERVER_HANDSHAKE) {

            TlsContext->ReceiveTPCallback(
                TlsContext->Connection,
                ServerMessage->SERVER_HANDSHAKE.QuicTPLength,
                ServerMessage->SERVER_HANDSHAKE.QuicTP);

            if (TlsContext->SecConfig->Flags & QUIC_CERTIFICATE_FLAG_DISABLE_CERT_VALIDATION) {
                LogWarning("[ tls][%p][%c] Certificate validation disabled!",
                    TlsContext, GetTlsIdentifier(TlsContext));
            } else {

                PQUIC_CERT ServerCert =
                    QuicCertParseChain(
                        ServerMessage->SERVER_HANDSHAKE.CertificateLength,
                        ServerMessage->SERVER_HANDSHAKE.Certificate);

                if (ServerCert == NULL) {
                    LogError("[ tls][%p][%c] Cert parse error.",
                        TlsContext, GetTlsIdentifier(TlsContext));
                    *ResultFlags |= QUIC_TLS_RESULT_ERROR;
                    break;
                }

                if (!QuicCertValidateChain(
                        ServerCert,
                        TlsContext->SNI,
                        TlsContext->SecConfig->Flags)) {
                    LogError("[ tls][%p][%c] Cert chain validation failed.",
                        TlsContext, GetTlsIdentifier(TlsContext));
                    *ResultFlags |= QUIC_TLS_RESULT_ERROR;
                    break;
                }
            }

            State->HandshakeComplete = TRUE;
            *ResultFlags |= QUIC_TLS_RESULT_COMPLETE;

            LogInfo("[ tls][%p][%c] Handshake complete.",
                TlsContext, GetTlsIdentifier(TlsContext));

            if (MaxClientMessageLength < MinMessageLengths[QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE]) {
                *ResultFlags |= QUIC_TLS_RESULT_ERROR;
                break;
            }

            uint16_t MessageLength = MinMessageLengths[QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE];
            TlsWriteUint24(ClientMessage->Length, MessageLength - 4);
            ClientMessage->Type = QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE;
            ClientMessage->CLIENT_HANDSHAKE.Success = TRUE;

            *ResultFlags |= QUIC_TLS_RESULT_DATA;
            State->BufferLength += MessageLength;
            State->BufferTotalLength += MessageLength;
            State->BufferOffset1Rtt = State->BufferTotalLength;

            *ResultFlags |= QUIC_TLS_RESULT_READ_KEY_UPDATED;
            State->ReadKey = QUIC_PACKET_KEY_1_RTT;
            State->ReadKeys[QUIC_PACKET_KEY_1_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_1_RTT);

            *ResultFlags |= QUIC_TLS_RESULT_WRITE_KEY_UPDATED;
            State->WriteKey = QUIC_PACKET_KEY_1_RTT;
            State->WriteKeys[QUIC_PACKET_KEY_1_RTT] = QuicStubAllocKey(QUIC_PACKET_KEY_1_RTT);

            TlsContext->LastMessageType = QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE;

        } else {
            LogError("[ tls][%p][%c] Invalid message, %u.",
                TlsContext, GetTlsIdentifier(TlsContext), ServerMessage->Type);
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        DrainLength = (uint16_t)TlsReadUint24(ServerMessage->Length) + 4;

        break;
    }

    case QUIC_TLS_MESSAGE_CLIENT_HANDSHAKE: {
        if (ServerMessage->Type != QUIC_TLS_MESSAGE_TICKET) {
            LogError("[ tls][%p][%c] Invalid message, %u.",
                TlsContext, GetTlsIdentifier(TlsContext), ServerMessage->Type);
            *ResultFlags |= QUIC_TLS_RESULT_ERROR;
            break;
        }

        *ResultFlags |= QUIC_TLS_RESULT_TICKET;
        TlsContext->TicketReady = TRUE;

        DrainLength = (uint16_t)TlsReadUint24(ServerMessage->Length) + 4;

        break;
    }

    default: {
        LogError("[ tls][%p][%c] Invalid last message, %u.",
            TlsContext, GetTlsIdentifier(TlsContext), TlsContext->LastMessageType);
        *ResultFlags |= QUIC_TLS_RESULT_ERROR;
        break;
    }
    }

    *BufferLength = DrainLength;
}

BOOLEAN
QuicTlsHasValidMessageToProcess(
    _In_ PQUIC_TLS TlsContext,
    _In_ uint32_t BufferLength,
    _In_reads_bytes_(BufferLength)
        const uint8_t* Buffer
    )
{
    if (!TlsContext->IsServer &&
        TlsContext->LastMessageType == QUIC_TLS_MESSAGE_INVALID &&
        BufferLength == 0) {
        return TRUE;
    }

    if (BufferLength < 7) {
        LogVerbose("[ tls][%p][%c] Insufficient data to process header.",
            TlsContext, GetTlsIdentifier(TlsContext));
        return FALSE;
    }
    
    const QUIC_FAKE_TLS_MESSAGE* Message = (QUIC_FAKE_TLS_MESSAGE*)Buffer;
    uint32_t MessageLength = TlsReadUint24(Message->Length) + 4;
    if (BufferLength < MessageLength) {
        LogVerbose("[ tls][%p][%c] Insufficient data to process %u bytes.",
            TlsContext, GetTlsIdentifier(TlsContext), MessageLength);
        return FALSE;
    }

    return TRUE;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_TLS_RESULT_FLAGS
QuicTlsProcessData(
    _In_ PQUIC_TLS TlsContext,
    _In_reads_bytes_(*BufferLength)
        const uint8_t * Buffer,
    _Inout_ uint32_t * BufferLength,
    _Inout_ QUIC_TLS_PROCESS_STATE* State
    )
{
    if (*BufferLength) {
        LogVerbose("[ tls][%p][%c] Processing %u received bytes.",
            TlsContext, GetTlsIdentifier(TlsContext), *BufferLength);
    }

    QUIC_TLS_RESULT_FLAGS ResultFlags = 0;

    if (QuicTlsHasValidMessageToProcess(TlsContext, *BufferLength, Buffer)) {

        uint16_t PrevBufferLength = State->BufferLength;
        if (TlsContext->IsServer) {
            QuicTlsServerProcess(TlsContext, &ResultFlags, State, BufferLength, Buffer);
        } else {
            QuicTlsClientProcess(TlsContext, &ResultFlags, State, BufferLength, Buffer);
        }

        LogInfo("[ tls][%p][%c] Consumed %u bytes.",
            TlsContext, GetTlsIdentifier(TlsContext), *BufferLength);

        if (State->BufferLength > PrevBufferLength) {
            LogInfo("[ tls][%p][%c] Produced %hu bytes.",
                TlsContext, GetTlsIdentifier(TlsContext), (State->BufferLength - PrevBufferLength));
        }

    } else {
        *BufferLength = 0;
    }

    return ResultFlags;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_TLS_RESULT_FLAGS
QuicTlsProcessDataComplete(
    _In_ PQUIC_TLS TlsContext,
    _Out_ uint32_t * BufferConsumed
    )
{
    UNREFERENCED_PARAMETER(TlsContext);
    UNREFERENCED_PARAMETER(BufferConsumed);
    return QUIC_TLS_RESULT_ERROR;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsReadTicket(
    _In_ PQUIC_TLS TlsContext,
    _Inout_ uint32_t* BufferLength,
    _Out_writes_bytes_opt_(*BufferLength)
        uint8_t* Buffer
    )
{
    if (!TlsContext->TicketReady) {
        return QUIC_STATUS_INVALID_STATE;
    } else if (*BufferLength == 0 || Buffer == NULL) {
        return QUIC_STATUS_BUFFER_TOO_SMALL;
    } else {
        Buffer[0] = 0xFF;
        *BufferLength = 1;
        return QUIC_STATUS_SUCCESS;
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsParamSet(
    _In_ PQUIC_TLS TlsContext,
    _In_ uint32_t Param,
    _In_ uint32_t BufferLength,
    _In_reads_bytes_(BufferLength)
        const void* Buffer
    )
{
    UNREFERENCED_PARAMETER(TlsContext);
    UNREFERENCED_PARAMETER(Param);
    UNREFERENCED_PARAMETER(BufferLength);
    UNREFERENCED_PARAMETER(Buffer);
    return QUIC_STATUS_NOT_SUPPORTED;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicTlsParamGet(
    _In_ PQUIC_TLS TlsContext,
    _In_ uint32_t Param,
    _Inout_ uint32_t* BufferLength,
    _Out_writes_bytes_opt_(*BufferLength)
        void* Buffer
    )
{
    UNREFERENCED_PARAMETER(TlsContext);
    UNREFERENCED_PARAMETER(Param);
    UNREFERENCED_PARAMETER(BufferLength);
    UNREFERENCED_PARAMETER(Buffer);
    return QUIC_STATUS_NOT_SUPPORTED;
}

//
// Crypto / Key Functionality
//

const uint64_t MAGIC_NO_ENCRYPTION_VALUE = 0xF0F1F2F3F4F5F6F7;

_IRQL_requires_max_(PASSIVE_LEVEL)
_When_(ReadKey != NULL, _At_(*ReadKey, __drv_allocatesMem(Mem)))
_When_(WriteKey != NULL, _At_(*WriteKey, __drv_allocatesMem(Mem)))
QUIC_STATUS
QuicPacketKeyCreateInitial(
    _In_ BOOLEAN IsServer,
    _In_reads_(QUIC_VERSION_SALT_LENGTH)
        const uint8_t* const Salt,  // Version Specific
    _In_ uint8_t CIDLength,
    _In_reads_(CIDLength)
        const uint8_t* const CID,
    _Out_opt_ QUIC_PACKET_KEY** ReadKey,
    _Out_opt_ QUIC_PACKET_KEY** WriteKey
    )
{
    UNREFERENCED_PARAMETER(IsServer);
    UNREFERENCED_PARAMETER(Salt);
    UNREFERENCED_PARAMETER(CIDLength);
    UNREFERENCED_PARAMETER(CID);
    if (ReadKey != NULL) {
        *ReadKey = QuicStubAllocKey(QUIC_PACKET_KEY_INITIAL);
    }
    if (WriteKey != NULL) {
        *WriteKey = QuicStubAllocKey(QUIC_PACKET_KEY_INITIAL);
    }
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
QuicPacketKeyFree(
    _In_opt_ __drv_freesMem(Mem) QUIC_PACKET_KEY* Key
    )
{
    if (Key != NULL) {
        QUIC_FREE(Key);
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_At_(*NewKey, __drv_allocatesMem(Mem))
QUIC_STATUS
QuicPacketKeyUpdate(
    _In_ QUIC_PACKET_KEY* OldKey,
    _Out_ QUIC_PACKET_KEY** NewKey
    )
{
    if (OldKey == NULL || OldKey->Type != QUIC_PACKET_KEY_1_RTT) {
        return QUIC_STATUS_INVALID_STATE;
    }
    *NewKey = QuicStubAllocKey(QUIC_PACKET_KEY_1_RTT);
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicKeyCreate(
    _In_ QUIC_AEAD_TYPE AeadType,
    _When_(AeadType == QUIC_AEAD_AES_128_GCM, _In_reads_(16))
    _When_(AeadType == QUIC_AEAD_AES_256_GCM, _In_reads_(32))
    _When_(AeadType == QUIC_AEAD_CHACHA20_POLY1305, _In_reads_(32))
        const uint8_t* const RawKey,
    _Out_ QUIC_KEY** NewKey
    )
{
    UNREFERENCED_PARAMETER(AeadType);
    UNREFERENCED_PARAMETER(RawKey);
    *NewKey = (QUIC_KEY*)0x1;
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
QuicKeyFree(
    _In_opt_ QUIC_KEY* Key
    )
{
    UNREFERENCED_PARAMETER(Key);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_STATUS
QuicEncrypt(
    _In_ QUIC_KEY* Key,
    _In_reads_bytes_(QUIC_IV_LENGTH)
        const uint8_t* const Iv,
    _In_ uint16_t AuthDataLength,
    _In_reads_bytes_opt_(AuthDataLength)
        const uint8_t* const AuthData,
    _In_ uint16_t BufferLength,
    _Inout_updates_bytes_(BufferLength)
        uint8_t* Buffer
    )
{
    UNREFERENCED_PARAMETER(Key);
    UNREFERENCED_PARAMETER(Iv);
    UNREFERENCED_PARAMETER(AuthDataLength);
    UNREFERENCED_PARAMETER(AuthData);
    uint16_t PlainTextLength = BufferLength - QUIC_ENCRYPTION_OVERHEAD;
    *(PUINT64)(Buffer + PlainTextLength) = MAGIC_NO_ENCRYPTION_VALUE;
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_STATUS
QuicDecrypt(
    _In_ QUIC_KEY* Key,
    _In_reads_bytes_(QUIC_IV_LENGTH)
        const uint8_t* const Iv,
    _In_ uint16_t AuthDataLength,
    _In_reads_bytes_opt_(AuthDataLength)
        const uint8_t* const AuthData,
    _In_ uint16_t BufferLength,
    _Inout_updates_bytes_(BufferLength)
        uint8_t* Buffer
    )
{
    UNREFERENCED_PARAMETER(Key);
    UNREFERENCED_PARAMETER(Iv);
    UNREFERENCED_PARAMETER(AuthDataLength);
    UNREFERENCED_PARAMETER(AuthData);
    uint16_t PlainTextLength = BufferLength - QUIC_ENCRYPTION_OVERHEAD;
    if (*(PUINT64)(Buffer + PlainTextLength) != MAGIC_NO_ENCRYPTION_VALUE) {
        return QUIC_STATUS_INVALID_PARAMETER;
    } else {
        return QUIC_STATUS_SUCCESS;
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicHpKeyCreate(
    _In_ QUIC_AEAD_TYPE AeadType,
    _When_(AeadType == QUIC_AEAD_AES_128_GCM, _In_reads_(16))
    _When_(AeadType == QUIC_AEAD_AES_256_GCM, _In_reads_(32))
    _When_(AeadType == QUIC_AEAD_CHACHA20_POLY1305, _In_reads_(32))
        const uint8_t* const RawKey,
    _Out_ QUIC_HP_KEY** NewKey
    )
{
    UNREFERENCED_PARAMETER(AeadType);
    UNREFERENCED_PARAMETER(RawKey);
    *NewKey = (QUIC_HP_KEY*)0x1;
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
QuicHpKeyFree(
    _In_opt_ QUIC_HP_KEY* Key
    )
{
    UNREFERENCED_PARAMETER(Key);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_STATUS
QuicHpComputeMask(
    _In_ QUIC_HP_KEY* Key,
    _In_ uint8_t BatchSize,
    _In_reads_bytes_(QUIC_HP_SAMPLE_LENGTH * BatchSize)
        const uint8_t* const Cipher,
    _Out_writes_bytes_(QUIC_HP_SAMPLE_LENGTH * BatchSize)
        uint8_t* Mask
    )
{
    UNREFERENCED_PARAMETER(Key);
    UNREFERENCED_PARAMETER(Cipher);
    QuicZeroMemory(Mask, BatchSize * QUIC_HP_SAMPLE_LENGTH);
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
QUIC_STATUS
QuicHashCreate(
    _In_ QUIC_HASH_TYPE HashType,
    _In_reads_(SaltLength)
        const uint8_t* const Salt,
    _In_ uint32_t SaltLength,
    _Out_ QUIC_HASH** NewHash
    )
{
    UNREFERENCED_PARAMETER(HashType);
    UNREFERENCED_PARAMETER(Salt);
    UNREFERENCED_PARAMETER(SaltLength);
    *NewHash = (QUIC_HASH*)0x1;
    return QUIC_STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
QuicHashFree(
    _In_opt_ QUIC_HASH* Hash
    )
{
    UNREFERENCED_PARAMETER(Hash);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_STATUS
QuicHashCompute(
    _In_ QUIC_HASH* Hash,
    _In_reads_(InputLength)
        const uint8_t* const Input,
    _In_ uint32_t InputLength,
    _In_ uint32_t OutputLength,
    _Out_writes_all_(OutputLength)
        uint8_t* const Output
    )
{
    UNREFERENCED_PARAMETER(Hash);
    UNREFERENCED_PARAMETER(Input);
    UNREFERENCED_PARAMETER(InputLength);
    UNREFERENCED_PARAMETER(OutputLength);
    UNREFERENCED_PARAMETER(Output);
    return QUIC_STATUS_SUCCESS;
}