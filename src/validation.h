// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Safe Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include "config/safe-config.h"
#endif

#include "amount.h"
#include "chain.h"
#include "coins.h"
#include "protocol.h" // For CMessageHeader::MessageStartChars
#include "script/script_error.h"
#include "sync.h"
#include "versionbits.h"
#include "spentindex.h"
#include "app/app.h"
#include "masternode.h"
#include "spos/spos.h"


#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <atomic>

#include <boost/unordered_map.hpp>
#include <boost/filesystem/path.hpp>

class CBlockIndex;
class CBlockTreeDB;
class CBloomFilter;
class CChainParams;
class CInv;
class CConnman;
class CScriptCheck;
class CTxMemPool;
class CValidationInterface;
class CValidationState;
class CMasternode;

struct LockPoints;

/** Default for accepting alerts from the P2P network. */
static const bool DEFAULT_ALERTS = true;
/** Default for DEFAULT_WHITELISTRELAY. */
static const bool DEFAULT_WHITELISTRELAY = true;
/** Default for DEFAULT_WHITELISTFORCERELAY. */
static const bool DEFAULT_WHITELISTFORCERELAY = true;
/** Default for -minrelaytxfee, minimum relay fee for transactions
 * We are ~100 times smaller then bitcoin now (2016-03-01), set minRelayTxFee only 10 times higher
 * so it's still 10 times lower comparing to bitcoin.
 * 2017-07: we are 10x smaller now, let's lower defaults 10x via the same BIP9 bit as DIP0001
 */
static const unsigned int DEFAULT_LEGACY_MIN_RELAY_TX_FEE = 10000; // was 1000
static const unsigned int DEFAULT_DIP0001_MIN_RELAY_TX_FEE = 1000;
/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/** Default for -mempoolexpiry, expiration time for mempool transactions in hours */
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 72;
/** The maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
static const unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
static const unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
/** Number of blocks that can be requested at any given time from a single peer. */
static const int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/** Timeout in seconds during which a peer must stall block download progress before being disconnected. */
static const unsigned int BLOCK_STALLING_TIMEOUT = 2;
/** Number of headers sent in one getheaders result. We rely on the assumption that if a peer sends
 *  less than this number, we reached its tip. Changing this value is a protocol upgrade. */
static const unsigned int MAX_HEADERS_RESULTS = 2000;
/** Size of the "block download window": how far ahead of our current height do we fetch?
 *  Larger windows tolerate larger download speed differences between peer, but increase the potential
 *  degree of disordering of blocks on disk (which make reindexing and in the future perhaps pruning
 *  harder). We'll probably want to make this a per-peer adaptive value at some point. */
static const unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/** Time to wait (in seconds) between writing blocks/block index to disk. */
static const unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/** Time to wait (in seconds) between flushing chainstate to disk. */
static const unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/** Maximum length of reject messages. */
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Average delay between local address broadcasts in seconds. */
static const unsigned int AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL = 24 * 24 * 60;
/** Average delay between peer address broadcasts in seconds. */
static const unsigned int AVG_ADDRESS_BROADCAST_INTERVAL = 30;
/** Average delay between trickled inventory broadcasts in seconds.
 *  Blocks, whitelisted receivers, and a random 25% of transactions bypass this. */
static const unsigned int AVG_INVENTORY_BROADCAST_INTERVAL = 5;
/** Block download timeout base, expressed in millionths of the block interval (i.e. 2.5 min) */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_BASE = 250000;
/** Additional block download timeout per parallel downloading peer (i.e. 1.25 min) */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_PER_PEER = 125000;

static const unsigned int DEFAULT_LIMITFREERELAY = 15;
static const bool DEFAULT_RELAYPRIORITY = true;

/** Default for -permitbaremultisig */
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
static const unsigned int DEFAULT_BYTES_PER_SIGOP = 20;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = true;
static const bool DEFAULT_ADDRESSINDEX = false;
static const bool DEFAULT_TIMESTAMPINDEX = false;
static const bool DEFAULT_SPENTINDEX = false;
static const unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;

static const bool DEFAULT_TESTSAFEMODE = false;
/** Default for -mempoolreplacement */
static const bool DEFAULT_ENABLE_REPLACEMENT = false;

/** Maximum number of headers to announce when relaying blocks with headers message.*/
static const unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetCheapHash(); }
};

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern CTxMemPool mempool;
typedef boost::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern BlockMap mapBlockIndex;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern const std::string strMessageMagic;
extern CWaitableCriticalSection csBestBlock;
extern CConditionVariable cvBlockChange;
extern bool fImporting;
extern bool fReindex;
extern int nScriptCheckThreads;
extern bool fTxIndex;
extern bool fIsBareMultisigStd;
extern bool fRequireStandard;
extern unsigned int nBytesPerSigOp;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
extern size_t nCoinCacheUsage;
extern CFeeRate minRelayTxFee;
extern bool fAlerts;
extern bool fEnableReplacement;

extern std::map<uint256, int64_t> mapRejectedBlocks;

static const int DIP0001_PROTOCOL_VERSION = 70208;
extern std::atomic<bool> fDIP0001WasLockedIn;
extern std::atomic<bool> fDIP0001ActiveAtTip;

extern int64_t g_nAllowableErrorTime;

/** Block hash whose ancestors we will assume to have valid scripts without checking them. */
extern uint256 hashAssumeValid;

/** Best header we've seen so far (used for getheaders queries' starting points). */
extern CBlockIndex *pindexBestHeader;

/** Minimum disk space required - used in CheckDiskSpace() */
static const uint64_t nMinDiskSpace = 52428800;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of chainActive.Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;

static const signed int DEFAULT_CHECKBLOCKS = MIN_BLOCKS_TO_KEEP;
static const unsigned int DEFAULT_CHECKLEVEL = 3;

// Require that user allocate at least 945MB for block & undo files (blk???.dat and rev???.dat)
// At 2MB per block, 288 blocks = 576MB.
// Add 15% for Undo data = 662MB
// Add 20% for Orphan block rate = 794MB
// We want the low water mark after pruning to be at least 794 MB and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128MB block file + added 15% undo data = 147MB greater for a total of 941MB
// Setting the target to > than 945MB will make it likely we can respect the target.
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 945 * 1024 * 1024;

/**
 * Process an incoming block. This only returns after the best known valid
 * block is made active. Note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 *
 * If you want to *possibly* get feedback on whether pblock is valid, you must
 * install a CValidationInterface (see validationinterface.h) - this will have
 * its BlockChecked method called whenever *any* block completes validation.
 *
 * Note that we guarantee that either the proof-of-work is valid on pblock, or
 * (and possibly also) BlockChecked will have been called.
 *
 * @param[in]   pblock  The block we want to process.
 * @param[in]   fForceProcessing Process this block even if unrequested; used for non-network block sources and whitelisted peers.
 * @param[out]  dbp     The already known disk position of pblock, or NULL if not yet stored.
 * @param[out]  fNewBlock A boolean which is set to indicate if the block was first received via this call
 * @return True if state.IsValid()
 */
bool ProcessNewBlock(const CChainParams& chainparams, const CBlock* pblock, bool fForceProcessing, const CDiskBlockPos* dbp, bool* fNewBlock);

/**
 * Process incoming block headers.
 *
 * @param[in]  block The block headers themselves
 * @param[out] state This may be set to an Error state if any error occurred processing them
 * @param[in]  chainparams The params for the chain we want to connect to
 * @param[out] ppindex If set, the pointer will be set to point to the last new block index object for the given headers
 */
bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex=NULL);

/** Check whether enough disk space is available for an incoming block */
bool CheckDiskSpace(uint64_t nAdditionalBytes = 0);
/** Open a block file (blk?????.dat) */
FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/** Open an undo file (rev?????.dat) */
FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/** Translation to a filesystem path */
boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix);
/** Import blocks from an external file */
bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp = NULL);
/** Initialize a new block tree database + block data on disk */
bool InitBlockIndex(const CChainParams& chainparams);
/** Load the block tree and coins database from disk */
bool LoadBlockIndex();
/** Unload database information */
void UnloadBlockIndex();
/** Run an instance of the script checking thread */
void ThreadScriptCheck();
/** Check whether we are doing an initial block download (synchronizing from disk or network) */
bool IsInitialBlockDownload();
/** Format a string that describes several potential problems detected by the core.
 * strFor can have three values:
 * - "rpc": get critical warnings, which should put the client in safe mode if non-empty
 * - "statusbar": get all warnings
 * - "gui": get all warnings, translated (where possible) for GUI
 * This function only returns the highest priority warning of the set selected by strFor.
 */
std::string GetWarnings(const std::string& strFor);
/** Retrieve a transaction (from memory pool, or from disk, if possible) */
bool GetTransaction(const uint256 &hash, CTransaction &tx, const Consensus::Params& params, uint256 &hashBlock, bool fAllowSlow = false);
/** Find the best known block, and make it the tip of the block chain */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, const CBlock* pblock = NULL);

double ConvertBitsToDouble(unsigned int nBits);
CAmount GetBlockSubsidy(int nBits, int nHeight, const Consensus::Params& consensusParams, bool fSuperblockPartOnly = false);
CAmount GetSPOSBlockSubsidy(int nPrevHeight, const Consensus::Params& consensusParams, bool fSuperblockPartOnly  = false);
CAmount GetMasternodePayment(int nHeight, CAmount blockValue);

int ConvertBlockHeight(const Consensus::Params& consensusParams);

int ConvertBlockParameterByHeight(const int& nHeight, const Consensus::Params& consensusParams);

int ConvertBlockConfirmationsByHeight(const int& nHeight);

int ConvertBlockNum();

int ConvertMasternodeConfirmationsByHeight(const int& nHeight, const Consensus::Params& consensusParams);

int ConvertSuperblockCycle(const int& nHeight);


/**
 * Prune block and undo files (blk???.dat and undo???.dat) so that the disk space used is less than a user-defined target.
 * The user sets the target (in MB) on the command line or in config file.  This will be run on startup and whenever new
 * space is allocated in a block or undo file, staying below the target. Changing back to unpruned requires a reindex
 * (which in this case means the blockchain must be re-downloaded.)
 *
 * Pruning functions are called from FlushStateToDisk when the global fCheckForPruning flag has been set.
 * Block and undo files are deleted in lock-step (when blk00003.dat is deleted, so is rev00003.dat.)
 * Pruning cannot take place until the longest chain is at least a certain length (100000 on mainnet, 1000 on testnet, 1000 on regtest).
 * Pruning will never delete a block within a defined distance (currently 288) from the active chain's tip.
 * The block index is updated by unsetting HAVE_DATA and HAVE_UNDO for any blocks that were stored in the deleted files.
 * A db flag records the fact that at least some block files have been pruned.
 *
 * @param[out]   setFilesToPrune   The set of file indices that can be unlinked will be returned
 */
void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight);

/**
 *  Actually unlink the specified files
 */
void UnlinkPrunedFiles(std::set<int>& setFilesToPrune);

/** Create a new block index entry for a given block hash */
CBlockIndex * InsertBlockIndex(uint256 hash);
/** Flush all state, indexes and buffers to disk. */
void FlushStateToDisk();
/** Prune block files and flush state to disk. */
void PruneAndFlush();

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransaction &tx, bool fLimitFree,
                        bool* pfMissingInputs, bool fOverrideMempoolLimit=false, bool fRejectAbsurdFee=false, bool fDryRun=false);

bool GetUTXOCoins(const COutPoint& outpoint, CCoins& coins);
int GetUTXOHeight(const COutPoint& outpoint);
int GetUTXOConfirmations(const COutPoint& outpoint);

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state);

/** Get the BIP9 state for a given deployment at the current tip. */
ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos);

////////////////////////////////////////////////////////////////////////////////////////
struct CName_Id_IndexValue
{
    uint256 id;
    int nHeight;

    CName_Id_IndexValue(const uint256& id = uint256(), const int& nHeight = 0) : id(id), nHeight(nHeight) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(id);
        READWRITE(nHeight);
    }
};

struct CAppId_AppInfo_IndexValue
{
    std::string strAdminAddress;
    CAppData appData;
    int nHeight;

    CAppId_AppInfo_IndexValue(const std::string& strAdminAddress = "", const CAppData& appData = CAppData(), const int& nHeight = 0)
        : strAdminAddress(strAdminAddress), appData(appData), nHeight(nHeight) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(LIMITED_STRING(strAdminAddress, MAX_ADDRESS_SIZE));
        READWRITE(appData);
        READWRITE(nHeight);
    }
};

struct CAuth_IndexKey
{
    uint256 appId;
    std::string strAddress;
    uint32_t nAuth;

    CAuth_IndexKey(const uint256& appId = uint256(), const std::string& strAddress = "", const uint32_t& nAuth = 0)
        : appId(appId), strAddress(strAddress), nAuth(nAuth) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(appId);
        READWRITE(LIMITED_STRING(strAddress, MAX_ADDRESS_SIZE));
        READWRITE(nAuth);
    }

    friend bool operator==(const CAuth_IndexKey&  a, const CAuth_IndexKey& b)
    {
        return (a.appId == b.appId && a.strAddress == b.strAddress && a.nAuth == b.nAuth);
    }

    friend bool operator<(const CAuth_IndexKey&  a, const CAuth_IndexKey& b)
    {
        if(a.appId == b.appId)
        {
            if(a.strAddress == b.strAddress)
                return a.nAuth < b.nAuth;
            return a.strAddress < b.strAddress;
        }
        return a.appId < b.appId;
    }
};

struct CAppTx_IndexKey
{
    uint256 appId;
    std::string strAddress;
    uint8_t nTxClass;
    COutPoint out;

    CAppTx_IndexKey(const uint256& appId = uint256(), const std::string& strAddress = "", const uint8_t& nTxClass = 0, const COutPoint& out = COutPoint())
        : appId(appId), strAddress(strAddress), nTxClass(nTxClass), out(out) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(appId);
        READWRITE(LIMITED_STRING(strAddress, MAX_ADDRESS_SIZE));
        READWRITE(nTxClass);
        READWRITE(out);
    }

    friend bool operator==(const CAppTx_IndexKey& a, const CAppTx_IndexKey& b)
    {
        return (a.appId == b.appId && a.strAddress == b.strAddress && a.nTxClass == b.nTxClass && a.out == b.out);
    }
};

struct CIterator_IdKey
{
    uint256 id;

    CIterator_IdKey(const uint256& id = uint256())      : id(id) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(id);
    }
};

struct CIterator_IdAddressKey
{
    uint256 id;
    std::string strAddress;

    CIterator_IdAddressKey(const uint256& id = uint256(), const std::string& strAddress = "") : id(id), strAddress(strAddress) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(id);
        READWRITE(LIMITED_STRING(strAddress, MAX_ADDRESS_SIZE));
    }
};

struct CAssetId_AssetInfo_IndexValue
{
    std::string strAdminAddress;
    CAssetData assetData;
    int nHeight;

    CAssetId_AssetInfo_IndexValue(const std::string& strAdminAddress = "", const CAssetData& assetData = CAssetData(), const int& nHeight = 0)
        : strAdminAddress(strAdminAddress), assetData(assetData), nHeight(nHeight) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(LIMITED_STRING(strAdminAddress, MAX_ADDRESS_SIZE));
        READWRITE(assetData);
        READWRITE(nHeight);
    }
};

struct CAssetTx_IndexKey
{
    uint256 assetId;
    std::string strAddress;
    uint8_t nTxClass;
    COutPoint out;

    CAssetTx_IndexKey(const uint256& assetId = uint256(), const std::string& strAddress = "", const uint8_t& nTxClass = 0, const COutPoint& out = COutPoint())
        : assetId(assetId), strAddress(strAddress), nTxClass(nTxClass), out(out) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetId);
        READWRITE(LIMITED_STRING(strAddress, MAX_ADDRESS_SIZE));
        READWRITE(nTxClass);
        READWRITE(out);
    }

    friend bool operator==(const CAssetTx_IndexKey& a, const CAssetTx_IndexKey& b)
    {
        return (a.assetId == b.assetId && a.strAddress == b.strAddress && a.nTxClass == b.nTxClass && a.out == b.out);
    }
};

struct CCandyInfo
{
    CAmount nAmount;
    uint16_t nExpired;

    CCandyInfo(const CAmount& nAmount = 0, const uint16_t& nExpired = 0) : nAmount(nAmount), nExpired(nExpired) {
    }

    CCandyInfo& operator=(const CCandyInfo& data)
    {
        if(this == &data)
            return *this;

        nAmount = data.nAmount;
        nExpired = data.nExpired;
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nAmount);
        READWRITE(nExpired);
    }

    friend bool operator==(const CCandyInfo& a, const CCandyInfo& b)
    {
        return (a.nAmount == b.nAmount && a.nExpired == b.nExpired);
    }

    friend bool operator<(const CCandyInfo& a, const CCandyInfo& b)
    {
        if(a.nAmount == b.nAmount)
            return a.nExpired < b.nExpired;
        return a.nAmount < b.nAmount;
    }
};

struct CPutCandy_IndexKey
{
    uint256 assetId;
    COutPoint out;
    CCandyInfo candyInfo;

    CPutCandy_IndexKey(const uint256& assetId = uint256(), const COutPoint& out = COutPoint(), const CCandyInfo& candyInfo = CCandyInfo()) : assetId(assetId), out(out), candyInfo(candyInfo) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetId);
        READWRITE(out);
        READWRITE(candyInfo);
    }

    friend bool operator==(const CPutCandy_IndexKey& a, const CPutCandy_IndexKey& b)
    {
        return (a.assetId == b.assetId && a.out == b.out && a.candyInfo == b.candyInfo);
    }

    friend bool operator<(const CPutCandy_IndexKey& a, const CPutCandy_IndexKey& b)
    {
        if(a.assetId == b.assetId)
        {
            if(a.out == b.out)
                return a.candyInfo < b.candyInfo;
            return a.out < b.out;
        }
        return a.assetId < b.assetId;
    }
};

struct CPutCandy_IndexValue
{
    int nHeight;
    uint256 blockHash;
    int nTxIndex;

    CPutCandy_IndexValue(const       int& nHeight = 0, const uint256& blockHash = uint256(), const int& nTxIndex = 0) : nHeight(nHeight), blockHash(blockHash), nTxIndex(nTxIndex) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nHeight);
        READWRITE(blockHash);
        READWRITE(nTxIndex);
    }

    friend bool operator==(const CPutCandy_IndexValue& a, const CPutCandy_IndexValue& b)
    {
        return (a.nHeight == b.nHeight && a.blockHash == b.blockHash && a.nTxIndex == b.nTxIndex);
    }
};

struct CIterator_IdOutKey
{
    uint256 assetId;
    COutPoint out;

    CIterator_IdOutKey(const uint256& assetId = uint256(), const COutPoint& out = COutPoint()) : assetId(assetId), out(out) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetId);
        READWRITE(out);
    }
};

struct CGetCandyCount_IndexKey
{
    uint256 assetId;
    COutPoint out;

    CGetCandyCount_IndexKey(const uint256& assetId = uint256(), const COutPoint& out = COutPoint())
        : assetId(assetId), out(out){
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetId);
        READWRITE(out);
    }

    friend bool operator==(const CGetCandyCount_IndexKey& a, const CGetCandyCount_IndexKey& b)
    {
        return (a.assetId == b.assetId && a.out == b.out);
    }

    friend bool operator<(const CGetCandyCount_IndexKey& a, const CGetCandyCount_IndexKey& b)
    {
        if(a.assetId == b.assetId)
            return a.out < b.out;
        return a.assetId < b.assetId;
    }
};

struct CGetCandyCount_IndexValue
{
    CAmount nGetCandyCount;

    CGetCandyCount_IndexValue(const CAmount& nGetCandyCount = 0)
        : nGetCandyCount(nGetCandyCount) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nGetCandyCount);
    }
};

struct CIterator_DeterministicMasternodeKey
{
    COutPoint out;

    CIterator_DeterministicMasternodeKey(const COutPoint& out=COutPoint()) : out(out) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(out);
    }
};

struct CDeterministicMasternode_IndexValue
{
    std::string strIP;
    uint16_t    nPort;
    std::string strCollateralAddress;
    std::string strSerialPubKeyId;
    int         nHeight;
    bool        fOfficial;
    COutPoint   currTxOut;
    COutPoint   lastTxOut;
    CDeterministicMasternode_IndexValue(const std::string& strIP="",const uint16_t& nPort=0,const std::string& strCollateralAddress="",const std::string& strSerialPubKeyId="",
                                        const int& nHeight=0,const bool& fOfficial=false,const COutPoint& currTxOut=COutPoint(),const COutPoint& lastTxOut=COutPoint())
                                        :strIP(strIP),nPort(nPort),strCollateralAddress(strCollateralAddress),strSerialPubKeyId(strSerialPubKeyId),
                                         nHeight(nHeight),fOfficial(fOfficial),currTxOut(currTxOut),lastTxOut(lastTxOut){
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(strIP);
        READWRITE(nPort);
        READWRITE(strCollateralAddress);
        READWRITE(strSerialPubKeyId);
        READWRITE(nHeight);
        READWRITE(fOfficial);
        READWRITE(lastTxOut);
        READWRITE(currTxOut);
    }
};

struct CGetCandy_IndexKey
{
    uint256 assetId;
    COutPoint out;
    std::string strAddress;

    CGetCandy_IndexKey(const uint256& assetId = uint256(), const COutPoint& out = COutPoint(), const std::string strAddress = "")
        : assetId(assetId), out(out), strAddress(strAddress) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(assetId);
        READWRITE(out);
        READWRITE(LIMITED_STRING(strAddress, MAX_ADDRESS_SIZE));
    }

    friend bool operator==(const CGetCandy_IndexKey& a, const CGetCandy_IndexKey& b)
    {
        return (a.assetId == b.assetId && a.out == b.out && a.strAddress == b.strAddress);
    }
};

struct CGetCandy_IndexValue
{
    CAmount nAmount;
    int nHeight;
    uint256 blockHash;
    int nTxIndex;

    CGetCandy_IndexValue(const CAmount& nAmount = 0, const int& nHeight = 0, const uint256& blockHash = uint256(), const int& nTxIndex = 0)
        : nAmount(nAmount), nHeight(nHeight), blockHash(blockHash), nTxIndex(nTxIndex) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(blockHash);
        READWRITE(nTxIndex);
    }
};

struct CIterator_MasternodePayeeKey
{
    std::string strPubKeyCollateralAddress;

    CIterator_MasternodePayeeKey(const std::string& strAddress = "") : strPubKeyCollateralAddress(strAddress) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(LIMITED_STRING(strPubKeyCollateralAddress, MAX_ADDRESS_SIZE));
    }
};

struct CMasternodePayee_IndexValue
{
    int nHeight;
    int64_t blockTime;
    int nPayeeTimes;
    CMasternodePayee_IndexValue(const int& height = 0,const int64_t& time=0,const int& paymentTimes=1)
        :nHeight(height),blockTime(time),nPayeeTimes(paymentTimes){
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nPayeeTimes);
        READWRITE(nHeight);
        READWRITE(blockTime);
    }
};

struct CSporkInfo_IndexValue
{
    int nStorageSpork;
    int nHeight;
    int nOfficialNum;
    int nGeneralNum;
    CSporkInfo_IndexValue(const int& nStorageSporkIn = 0, const int& nHeightIn = 0, const int& nOfficialNumIn = 0, const int& nGeneralNumIn = 0):
                                nStorageSpork(nStorageSporkIn), nHeight(nHeightIn), nOfficialNum(nOfficialNumIn), nGeneralNum(nGeneralNumIn)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nStorageSpork);
        READWRITE(nHeight);
        READWRITE(nOfficialNum);
        READWRITE(nGeneralNum);
    }
    
};

struct CIterator_SporkInfo_IndexValue
{
    int nStorageSpork;

    CIterator_SporkInfo_IndexValue(const int& nStorageSporkIn = 0) : nStorageSpork(nStorageSporkIn) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nStorageSpork);
    }
};

struct CHeight_IndexKey
{
    int nHeight;

    CHeight_IndexKey(const int &nHeight = 0) : nHeight(nHeight) {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nHeight);
    }
};

struct CCandy_BlockTime_Info
{
    uint256 assetId;
    CAssetData assetData;
    CCandyInfo candyinfo;
    COutPoint outpoint;
    int64_t blocktime;
    int nHeight;

    CCandy_BlockTime_Info(const uint256& assetIdIn, const CAssetData& assetDataIn, const CCandyInfo& candyinfoIn, const COutPoint& outpointIn, const int64_t& blocktimeIn,int height)
    {
        assetId = assetIdIn;
        assetData = assetDataIn;
        candyinfo = candyinfoIn;
        outpoint = outpointIn;
        blocktime = blocktimeIn;
        nHeight = height;
    }

    CCandy_BlockTime_Info& operator=(const CCandy_BlockTime_Info& info)
    {
        if(this == &info)
            return *this;

        assetId = info.assetId;
        assetData = info.assetData;
        candyinfo = info.candyinfo;
        outpoint = info.outpoint;
        blocktime = info.blocktime;
        nHeight = info.nHeight;
        return *this;
    }
};

struct CCandy_BlockTime_InfoVec
{
    std::vector<CCandy_BlockTime_Info> vallcandyinfovec;
};

////////////////////////////////////////////////////////////////////////////////////////
struct CChangeInfo
{
    int nHeight;
    int nLastCandyHeight;
    CAmount nReward;
    bool fCandy;
    std::map<std::string, CAmount> mapAddressAmount;

    CChangeInfo(const int& nHeight = 0, const int& nLastCandyHeight = 0, const CAmount& nReward = 0, const bool fCandy = false, const std::map<std::string, CAmount>& mapAddressAmount = (std::map<std::string, CAmount>()))
        : nHeight(nHeight), nLastCandyHeight(nLastCandyHeight), nReward(nReward), fCandy(fCandy), mapAddressAmount(mapAddressAmount) {
    }

    CChangeInfo& operator=(const CChangeInfo& data)
    {
        if(this == &data)
            return *this;

        nHeight = data.nHeight;
        nLastCandyHeight = data.nLastCandyHeight;
        nReward = data.nReward;
        fCandy = data.fCandy;
        mapAddressAmount = data.mapAddressAmount;

        return *this;
    }

    friend bool operator==(const CChangeInfo& a, const CChangeInfo& b)
    {
        return a.nHeight == b.nHeight;
    }

    friend bool operator<=(const CChangeInfo& a, const CChangeInfo& b)
    {
        return a.nHeight <= b.nHeight;
    }
};

struct CBlockDetail
{
    int nHeight;
    int nLastCandyHeight;
    CAmount nReward;
    CAmount nFilterAmount;
    bool fCandy;

    CBlockDetail(const int& nHeight = 0, const int& nLastCandyHeight = 0, const CAmount& nReward = 0, const CAmount& nFilterAmount = 0, const bool fCandy = false)
        : nHeight(nHeight), nLastCandyHeight(nLastCandyHeight), nReward(nReward), nFilterAmount(nFilterAmount), fCandy(fCandy) {
    }

    std::string ToString()
    {
        return strprintf("%d: %d, %lld, %lld, %s", nHeight, nLastCandyHeight, nReward, nFilterAmount, fCandy ? "candy" : "non-candy");
    }

    friend bool operator>(const CBlockDetail& a, const CBlockDetail& b)
    {
        return a.nHeight > b.nHeight;
    }

    friend bool operator<(const CBlockDetail& a, const CBlockDetail& b)
    {
        return a.nHeight < b.nHeight;
    }

    friend bool operator==(const CBlockDetail& a, const CBlockDetail& b)
    {
        return a.nHeight == b.nHeight;
    }
};

struct CAddressAmount
{
    char szAddress[36];
    CAmount nAmount;

    CAddressAmount()
    {
        memset(szAddress, 0x00, sizeof(szAddress));
        nAmount = 0;
    }

    CAddressAmount(const char* szAddressIn, const CAmount& nAmountIn)
    {
        memset(szAddress, 0x00, sizeof(szAddress));
        strncpy(szAddress, szAddressIn, sizeof(szAddress) - 1);
        nAmount = nAmountIn;
    }

    CAddressAmount(const std::string& strAddressIn, const CAmount& nAmountIn)
    {
        memset(szAddress, 0x00, sizeof(szAddress));
        strncpy(szAddress, strAddressIn.data(), sizeof(szAddress) - 1);
        nAmount = nAmountIn;
    }

    CAddressAmount& operator=(const CAddressAmount& data)
    {
        if(this == &data)
            return *this;

        memset(szAddress, 0x00, sizeof(szAddress));
        strncpy(szAddress, data.szAddress, sizeof(szAddress) - 1);
        nAmount = data.nAmount;

        return *this;
    }

    friend bool operator>(const CAddressAmount& a, const CAddressAmount& b)
    {
        return strcmp(a.szAddress, b.szAddress) > 0;
    }

    friend bool operator<(const CAddressAmount& a, const CAddressAmount& b)
    {
        return strcmp(a.szAddress, b.szAddress) < 0;
    }

    friend bool operator==(const CAddressAmount& a, const CAddressAmount& b)
    {
        return strcmp(a.szAddress, b.szAddress) == 0;
    }
};

////////////////////////////////////////////////////////////////////////////////////////
struct CTimestampIndexIteratorKey {
    unsigned int timestamp;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 4;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata32be(s, timestamp);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        timestamp = ser_readdata32be(s);
    }

    CTimestampIndexIteratorKey(unsigned int time) {
        timestamp = time;
    }

    CTimestampIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
    }
};

struct CTimestampIndexKey {
    unsigned int timestamp;
    uint256 blockHash;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 36;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata32be(s, timestamp);
        blockHash.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        timestamp = ser_readdata32be(s);
        blockHash.Unserialize(s, nType, nVersion);
    }

    CTimestampIndexKey(unsigned int time, uint256 hash) {
        timestamp = time;
        blockHash = hash;
    }

    CTimestampIndexKey() {
        SetNull();
    }

    void SetNull() {
        timestamp = 0;
        blockHash.SetNull();
    }
};

struct CAddressUnspentKey {
    unsigned int type;
    uint160 hashBytes;
    uint256 txhash;
    size_t index;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 57;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s, nType, nVersion);
        txhash.Serialize(s, nType, nVersion);
        ser_writedata32(s, index);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
        txhash.Unserialize(s, nType, nVersion);
        index = ser_readdata32(s);
    }

    CAddressUnspentKey(unsigned int addressType, uint160 addressHash, uint256 txid, size_t indexValue) {
        type = addressType;
        hashBytes = addressHash;
        txhash = txid;
        index = indexValue;
    }

    CAddressUnspentKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        txhash.SetNull();
        index = 0;
    }
};

struct CAddressUnspentValue {
    CAmount satoshis;
    CScript script;
    int blockHeight;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(satoshis);
        READWRITE(*(CScriptBase*)(&script));
        READWRITE(blockHeight);
    }

    CAddressUnspentValue(CAmount sats, CScript scriptPubKey, int height) {
        satoshis = sats;
        script = scriptPubKey;
        blockHeight = height;
    }

    CAddressUnspentValue() {
        SetNull();
    }

    void SetNull() {
        satoshis = -1;
        script.clear();
        blockHeight = 0;
    }

    bool IsNull() const {
        return (satoshis == -1);
    }
};

struct CAddressIndexKey {
    unsigned int type;
    uint160 hashBytes;
    int blockHeight;
    unsigned int txindex;
    uint256 txhash;
    size_t index;
    bool spending;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 66;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s, nType, nVersion);
        // Heights are stored big-endian for key sorting in LevelDB
        ser_writedata32be(s, blockHeight);
        ser_writedata32be(s, txindex);
        txhash.Serialize(s, nType, nVersion);
        ser_writedata32(s, index);
        char f = spending;
        ser_writedata8(s, f);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
        blockHeight = ser_readdata32be(s);
        txindex = ser_readdata32be(s);
        txhash.Unserialize(s, nType, nVersion);
        index = ser_readdata32(s);
        char f = ser_readdata8(s);
        spending = f;
    }

    CAddressIndexKey(unsigned int addressType, uint160 addressHash, int height, int blockindex,
                     uint256 txid, size_t indexValue, bool isSpending) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
        txindex = blockindex;
        txhash = txid;
        index = indexValue;
        spending = isSpending;
    }

    CAddressIndexKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
        txindex = 0;
        txhash.SetNull();
        index = 0;
        spending = false;
    }

};

struct CAddressIndexIteratorKey {
    unsigned int type;
    uint160 hashBytes;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 21;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
    }

    CAddressIndexIteratorKey(unsigned int addressType, uint160 addressHash) {
        type = addressType;
        hashBytes = addressHash;
    }

    CAddressIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
    }
};

struct CAddressIndexIteratorHeightKey {
    unsigned int type;
    uint160 hashBytes;
    int blockHeight;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 25;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s, nType, nVersion);
        ser_writedata32be(s, blockHeight);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
        blockHeight = ser_readdata32be(s);
    }

    CAddressIndexIteratorHeightKey(unsigned int addressType, uint160 addressHash, int height) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
    }

    CAddressIndexIteratorHeightKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
    }
};

struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

struct CFirstBlockInfo
{
    CDeterministicMNCoinbaseData deterministicMNCoinbaseData;
    int nHeight;
    uint32_t nForwardTime;

    CFirstBlockInfo(const CDeterministicMNCoinbaseData& deterministicMNCoinbaseDataIn, const int& nHeightIn, const uint32_t& nForwardTimeIn)
                       : deterministicMNCoinbaseData(deterministicMNCoinbaseDataIn), nHeight(nHeightIn), nForwardTime(nForwardTimeIn){
    }

    CFirstBlockInfo& operator=(const CFirstBlockInfo& data)
    {
        if(this == &data)
            return *this;

        deterministicMNCoinbaseData = data.deterministicMNCoinbaseData;
        nHeight = data.nHeight;
        nForwardTime = data.nForwardTime;

        return *this;
    }

    CFirstBlockInfo()
    {
        SetNull();
    }

    void SetNull()
    {
        deterministicMNCoinbaseData.SetNull();
        nHeight = 0;
        nForwardTime = 0;
    }
};

/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 *
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);


/**
 * Check whether all inputs of this transaction are valid (no double spends, scripts & sigs, amounts)
 * This does not modify the UTXO set. If pvChecks is not NULL, script checks are pushed onto it
 * instead of being performed inline.
 */
bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &view, bool fScriptChecks,
                 unsigned int flags, bool cacheStore, std::vector<CScriptCheck> *pvChecks = NULL);

/** Apply the effects of this transaction on the UTXO set represented by view */
void UpdateCoins(const CTransaction& tx, CValidationState &state, CCoinsViewCache &inputs, int nHeight);

enum CTxSrcType{
    FROM_BLOCK,
    FROM_WALLET,
    FROM_NEW
};

enum SPORK_SELECT_LOOP
{
  NO_SPORK_SELECT_LOOP = 0,
  SPORK_SELECT_LOOP_1 = 1,
  SPORK_SELECT_LOOP_2 = 2,
  SPORK_SELECT_LOOP_OVER_TIMEOUT_LIMIT = 3
};

bool CheckUnlockedHeight(const int32_t& nTxVersion, const int64_t& nOffset);

/** Context-independent validity checks */
bool CheckTransaction(const CTransaction& tx, CValidationState& state, const enum CTxSrcType& nType, const int& nHeight = -1);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

/**
 * Check if transaction will be final in the next block to be created.
 *
 * Calls IsFinalTx() with current block height and appropriate block time.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckFinalTx(const CTransaction &tx, int flags = -1);

/**
 * Test whether the LockPoints height and time are still valid on the current chain
 */
bool TestLockPointValidity(const LockPoints* lp);

/**
 * Check if transaction is final per BIP 68 sequence numbers and can be included in a block.
 * Consensus critical. Takes as input a list of heights at which tx's inputs (in order) confirmed.
 */
bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block);

/**
 * Check if transaction will be BIP 68 final in the next block to be created.
 *
 * Simulates calling SequenceLocks() with data from the tip of the current active chain.
 * Optionally stores in LockPoints the resulting height and time calculated and the hash
 * of the block needed for calculation or skips the calculation and uses the LockPoints
 * passed in for evaluation.
 * The LockPoints should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(const CTransaction &tx, int flags, LockPoints* lp = NULL, bool useExistingLockPoints = false);

/**
 * Closure representing one script verification
 * Note that this stores references to the spending transaction
 */
class CScriptCheck
{
private:
    CScript scriptPubKey;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    ScriptError error;

public:
    CScriptCheck(): ptxTo(0), nIn(0), nFlags(0), cacheStore(false), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CCoins& txFromIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn) :
        scriptPubKey(txFromIn.vout[txToIn.vin[nInIn].prevout.n].scriptPubKey),
        ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), error(SCRIPT_ERR_UNKNOWN_ERROR) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        scriptPubKey.swap(check.scriptPubKey);
        std::swap(ptxTo, check.ptxTo);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
    }

    ScriptError GetScriptError() const { return error; }
};

bool GetTimestampIndex(const unsigned int &high, const unsigned int &low, std::vector<uint256> &hashes);
bool GetSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);
bool GetAddressIndex(uint160 addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
                     int start = 0, int end = 0);
bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs);

/** Functions for disk access for blocks */
bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart);
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams);

/** Functions for validating blocks and updating the block tree */

/** Undo the effects of this block (with given index) on the UTXO set represented by coins.
 *  In case pfClean is provided, operation will try to be tolerant about errors, and *pfClean
 *  will be true if no problems were found. Otherwise, the return value will be false in case
 *  of problems. Note that in any case, coins may be modified. */
bool DisconnectBlock(const CBlock& block, CValidationState& state, const CBlockIndex* pindex, CCoinsViewCache& coins, bool* pfClean = NULL);

/** Reprocess a number of blocks to try and get on the correct chain again **/
bool DisconnectBlocks(int blocks);
void ReprocessBlocks(int nBlocks);

/** Apply the effects of this block (with given index) on the UTXO set represented by coins */
bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& coins, bool fJustCheck = false);

/** Context-independent validity checks */
bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW = true);
bool CheckBlock(const CBlock& block, const int& nHeight, CValidationState& state, bool fCheckPOW = true, bool fCheckMerkleRoot = true);
bool CheckSPOSBlock(const CBlock& block, CValidationState& state, const int& nHeight,bool fCheckPOW = true);
bool CheckSPOSBlockV2(const CBlock& block, CValidationState& state, const int& nHeight, const std::vector<unsigned char>& vData, bool fCheckSPOSIndex = true);
bool DealDeterministicMNCoinBaseReserve(const CBlock& block, CBlockIndex* pindex, bool fCheckFail);
bool DealMemAndDBSpork(CBlockIndex* pindex);
bool LoadSporkInfo();
bool StorageSporkInfo(const CSporkInfo_IndexValue& sporkInfoValue);


/** Context-dependent validity checks */
bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex *pindexPrev);
bool ContextualCheckBlock(const CBlock& block, CValidationState& state, CBlockIndex *pindexPrev);

/** Check a block is completely valid from start to finish (only works on top of our current best block, with cs_main held) */
bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/**Get a map of the amount corresponding to the address according to the height*/
bool GetAddressAmountByHeight(const int& nHeight, const std::string& strAddress, CAmount& nAmount);
bool GetTotalAmountByHeight(const int& nHeight, CAmount& nTotalAmount);

class CBlockFileInfo
{
public:
    unsigned int nBlocks;      //! number of blocks stored in file
    unsigned int nSize;        //! number of used bytes of block file
    unsigned int nUndoSize;    //! number of used bytes in the undo file
    unsigned int nHeightFirst; //! lowest height of block in file
    unsigned int nHeightLast;  //! highest height of block in file
    uint64_t nTimeFirst;         //! earliest time of block in file
    uint64_t nTimeLast;          //! latest time of block in file

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nBlocks));
        READWRITE(VARINT(nSize));
        READWRITE(VARINT(nUndoSize));
        READWRITE(VARINT(nHeightFirst));
        READWRITE(VARINT(nHeightLast));
        READWRITE(VARINT(nTimeFirst));
        READWRITE(VARINT(nTimeLast));
    }

     void SetNull() {
         nBlocks = 0;
         nSize = 0;
         nUndoSize = 0;
         nHeightFirst = 0;
         nHeightLast = 0;
         nTimeFirst = 0;
         nTimeLast = 0;
     }

     CBlockFileInfo() {
         SetNull();
     }

     std::string ToString() const;

     /** update statistics (does not update nSize) */
     void AddBlock(unsigned int nHeightIn, uint64_t nTimeIn) {
         if (nBlocks==0 || nHeightFirst > nHeightIn)
             nHeightFirst = nHeightIn;
         if (nBlocks==0 || nTimeFirst > nTimeIn)
             nTimeFirst = nTimeIn;
         nBlocks++;
         if (nHeightIn > nHeightLast)
             nHeightLast = nHeightIn;
         if (nTimeIn > nTimeLast)
             nTimeLast = nTimeIn;
     }
};

/** RAII wrapper for VerifyDB: Verify consistency of the block and coin databases */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth);
};

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator);

/** Mark a block as invalid. */
bool InvalidateBlock(CValidationState& state, const Consensus::Params& consensusParams, CBlockIndex *pindex);

/** Remove invalidity status from a block and its descendants. */
bool ReconsiderBlock(CValidationState& state, CBlockIndex *pindex);

/** The currently-connected chain of blocks (protected by cs_main). */
extern CChain chainActive;

/** Global variable that points to the active CCoinsView (protected by cs_main) */
extern CCoinsViewCache *pcoinsTip;

/** Global variable that points to the active block tree (protected by cs_main) */
extern CBlockTreeDB *pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache& inputs);

extern VersionBitsCache versionbitscache;

/**
 * Determine what nVersion a new block should use.
 */
int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params, bool fAssumeMasternodeIsUpgraded = false);

/**
 * Return true if hash can be found in chainActive at nBlockHeight height.
 * Fills hashRet with found hash, if no nBlockHeight is specified - chainActive.Height() is used.
 */
bool GetBlockHash(uint256& hashRet, int nBlockHeight = -1);

/** Reject codes greater or equal to this can be returned by AcceptToMemPool
 * for transactions, to signal internal conditions. They cannot and should not
 * be sent over the P2P network.
 */
static const unsigned int REJECT_INTERNAL = 0x100;
/** Too high fee. Can not be triggered by P2P transactions */
static const unsigned int REJECT_HIGHFEE = 0x100;
/** Transaction is already known (either in mempool or blockchain) */
static const unsigned int REJECT_ALREADY_KNOWN = 0x101;
/** Transaction conflicts with a transaction already known */
static const unsigned int REJECT_CONFLICT = 0x102;

bool GetAppInfoByAppId(const uint256& appId, CAppId_AppInfo_IndexValue& appInfo, const bool fWithMempool = true);
bool GetAppIdByAppName(const std::string& strAppName, uint256& appId, const bool fWithMempool = true);
bool GetTxInfoByAppId(const uint256& appId, std::vector<COutPoint>& vOut, const bool fWithMempool = true);
bool GetTxInfoByAppIdAddress(const uint256& appId, const std::string& strAddress, std::vector<COutPoint>& vOut, const bool fWithMempool = true);
bool GetAppListInfo(std::vector<uint256> &vappid, const bool fWithMempool = true);
bool GetAppIDListByAddress(const std::string &strAddress, std::vector<uint256> &appIdlist, const bool fWithMempool = true);
bool GetExtendDataByTxId(const uint256& txId, std::vector<std::pair<uint256, std::string> > &vExtendData);
bool GetAuthByAppIdAddress(const uint256& appId, const std::string& strAddress, std::map<uint32_t, int>& mapAuth);
bool GetAuthByAppIdAddressFromMempool(const uint256& appId, const std::string& strAddress, std::vector<uint32_t>& vAuth);
bool GetAssetInfoByAssetId(const uint256& assetId, CAssetId_AssetInfo_IndexValue& assetInfo, const bool fWithMempool = true);
bool GetAssetIdByShortName(const std::string& strShortName, uint256& assetId, const bool fWithMempool = true);
bool GetAssetIdByAssetName(const std::string& strAssetName, uint256& assetId, const bool fWithMempool = true);
bool GetTxInfoByAssetIdTxClass(const uint256& assetId, const uint8_t& nTxClass, std::vector<COutPoint>& vOut, const bool fWithMempool = true);
bool GetTxInfoByAssetIdAddressTxClass(const uint256& assetId, const std::string& strAddress, const uint8_t& nTxClass, std::vector<COutPoint>& vOut, const bool fWithMempool = true);
bool GetAssetIdByAddress(const std::string & strAddress, std::vector<uint256> &assetIdlist, const bool fWithMempool = true);
bool GetAssetIdCandyInfo(const uint256& assetId, std::map<COutPoint, CCandyInfo>& mapCandyInfo);
bool GetAssetIdCandyInfo(const uint256& assetId, const COutPoint& out, CCandyInfo& candyInfo);
bool GetGetCandyAmount(const uint256& assetId, const COutPoint& out, const std::string& strAddress, CAmount& amount, const bool fWithMempool = true);
bool GetGetCandyTotalAmount(const uint256& assetId, const COutPoint& out, CAmount& dbamount, CAmount& memamount, const bool fWithMempool = true);
bool GetAssetListInfo(std::vector<uint256> &vAssetId, const bool fWithMempool = true);
bool GetIssueAssetInfo(std::map<uint256, CAssetData> &mapissueassetinfo);
CAmount GetAddedAmountByAssetId(const uint256& assetId, const bool fWithMempool = true);
bool GetDeterministicMasternodeByCOutPoint(const COutPoint& out,CDeterministicMasternode_IndexValue& dmn,const bool fWithMempool = true);

void ThreadGetAllCandyInfo();
void ThreadWriteChangeInfo();
void ThreadCalculateAddressAmount();
bool VerifyDetailFile();
bool LoadChangeInfoToList();
bool LoadCandyHeightToList();

bool GetCOutPointAddress(const uint256& assetId, std::map<COutPoint, std::vector<std::string>> &moutpointaddress);
bool GetCOutPointList(const uint256& assetId, const std::string& strAddress, std::vector<COutPoint> &vcoutpoint);

bool GetAssetIdCandyInfoList(std::map<CPutCandy_IndexKey, CPutCandy_IndexValue>& mapCandy);

std::string getNumString(int* num);
void resetNumA(std::string numAStr);
void resetNumB(std::string numBStr);
int compareFloatString(const std::string& numAStr,const std::string& numBStr,bool fOnlyCompareInt=true);
int comparestring(std::string numAStr,std::string numBStr);
std::string plusstring(std::string numAStr, std::string numBStr);
std::string minusstring(std::string numAStr, std::string numBStr);
std::string mulstring(std::string numAStr, std::string numBStr);
std::string numtofloatstring(std::string numstr, int32_t Decimals);

bool ExistForbidTxin(const int nHeight, const std::vector<int>& prevheights);

bool CompareGetCandyPutCandyTotal(std::map<CPutCandy_IndexKey, CAmount> &mapAssetGetCandy, const CPutCandy_IndexKey &key, const CAmount &ngetcandytotalamount, const CAmount &nputcandytotalamount, const CAmount &nCandyAmount, CAmount &nmapgetcandyamount);
bool CompareDBGetCandyPutCandyTotal(std::map<CPutCandy_IndexKey, CAmount> &mapAssetGetCandy, const CPutCandy_IndexKey &key, const CAmount &ndbgetcandytotalamount, const CAmount &nputcandytotalamount, const CAmount &nCandyAmount, CAmount &nmapgetcandyamount);

void UpdateMasternodeGlobalData(const std::vector<CMasternode>& tmpVecMasternodes,bool bClearVec,int selectMasterNodeRet,int64_t nStartNewLoopTime);


void UpdateGlobalTimeoutCount(int nTimeoutCount);
void UpdateGlobalReceiveBlock(bool fReceiveBlock);
void SelectMasterNodeByPayee(int nCurrBlockHeight, uint32_t nTime,uint32_t nScoreTime, const bool bSpork, const bool bProcessSpork,std::vector<CMasternode>& tmpVecResultMasternodes
                             ,bool& bClearVec,int& nSelectMasterNodeRet,int64_t& nStartNewLoopTime,bool fTimeoutReselect,
                             const unsigned int& nMasternodeSPosCount, SPORK_SELECT_LOOP nSporkSelectLoop, bool fRemoveOfficialMasternode = false);

void SelectDeterministicMN(const int& nCurrBlockHeight, const uint32_t& nTime, const uint32_t& nScoreTime, const bool& bProcessSpork, std::vector<CDeterministicMasternode_IndexValue>& tmpVecResultMasternodes,
                                 int& nSelectMasterNodeRet, int64_t& nStartNewLoopTime, bool fTimeoutReselect, const unsigned int& nOfficialCount);
void GetEffectiveGeneralMNData(const std::map<COutPoint, CDeterministicMasternode_IndexValue>& mapAllEffectiveMasterNode, const std::map<std::string, CMasternodePayee_IndexValue>& mapAllEffectivePayeeInfo,
                                       std::map<COutPoint, CDeterministicMasternode_IndexValue> &mapEffectiveGeneralMNs);
void GetEffectiveDeterministicMNData(const std::map<COutPoint, CDeterministicMasternode_IndexValue>& mapAllMasterNode, const int& nHeight, std::map<COutPoint, CDeterministicMasternode_IndexValue> &mapEffectiveMasternode);
void GetEffectivePayeeData(const std::map<std::string, CMasternodePayee_IndexValue>& mapAllPayeeInfo, const int& nHeight, std::map<std::string, CMasternodePayee_IndexValue>& mapAllEffectivePayeeInfo);
void GetEffectiveOfficialMNData(const std::map<COutPoint, CDeterministicMasternode_IndexValue> &mapAllOfficialMNs, std::map<COutPoint, CDeterministicMasternode_IndexValue> &mapEffectiveOfficialMNs);
void SortDeterministicMNs(const std::map<COutPoint, CDeterministicMasternode_IndexValue> &mapMasternodes, std::vector<CDeterministicMasternode_IndexValue>& vecResultMasternodes, const uint32_t& nScoreTime, const std::string& strArrName);
void UpdateReSelectMNGlobalData(const std::vector<CDeterministicMasternode_IndexValue>& tmpVecMasternodes);
void UpdateDeterministicMNGlobalData(const std::vector<CDeterministicMasternode_IndexValue>& tmpVecMasternodes, const int& selectMasterNodeRet, const int64_t& nStartNewLoopTime);
void InitReSelectMNGlobalData();
void InitDeterministicMNGlobalData();
void InitMasternodeGlobalData();
void ReSelectDeterministicMN(const int& nCurrBlockHeight, const uint32_t& nScoreTime, const unsigned int& nOfficialCount, std::vector<CDeterministicMasternode_IndexValue>& tmpVecResultMasternodes);
bool GetDeterministicMNList(const int& nCurrBlockHeight, const uint32_t& nScoreTime, std::vector<CDeterministicMasternode_IndexValue>& tmpVecResultMasternodes, const unsigned int& nOfficialCount, int& nSelectMasterNodeRet);

bool CompareBestChainActiveTime(const CBlockIndex *pCurrentBlockIndex, const CBlockIndex *pBestBlockIndex, const bool fComEquals = false);
void updateForwardHeightAndScoreHeight(int nCurrBlockHeight,int& nForwardHeight,int& nScoreHeight);

bool GetSporkInfo(const int& nStorageSpork, CSporkInfo_IndexValue& value);
bool EraseSporkInfo(const int& nStorageSpork);
bool WriteSporkInfo(const int& nStorageSpork, const CSporkInfo_IndexValue& value);

void GetAllDeterministicMasternodeMap(std::map<COutPoint,CDeterministicMasternode_IndexValue>& mapOfficialDeterministicMasternode,
                   std::map<COutPoint,CDeterministicMasternode_IndexValue>& mapAllDeterministicMasternode,bool fSaveCommon=false);

void LoadSPOSInfo();


#endif // BITCOIN_VALIDATION_H
