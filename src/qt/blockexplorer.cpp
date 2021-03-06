// Copyright (c) 2020 The Gapcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QDateTime>
#include <QKeyEvent>
#include <QMessageBox>
#include <set>
#include <qt/blockexplorer.h>
#include <qt/forms/ui_blockexplorer.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>

#include <base58.h>
#include <bignum.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <core_io.h>
#include <net.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <txdb.h>
#include <ui_interface.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validation.h>
#include <wallet/rpcwallet.h>

extern double GetDifficulty(const CBlockIndex* blockindex = NULL);

static PoWUtils *utils = new PoWUtils();

inline std::string utostr(unsigned int n)
{
    return strprintf("%u", n);
}

static std::string makeHRef(const std::string& Str)
{
    return "<a href=\"" + Str + "\">" + Str + "</a>";
}

static int64_t getTxIn(const CTransaction& tx)
{
    if (tx.IsCoinBase())
        return 0;

    int64_t Sum = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        Sum += getPrevOut(tx.vin[i].prevout).nValue;
    return Sum;
}

static std::string ValueToString(int64_t nValue, bool AllowNegative = false)
{
    if (nValue < 0 && !AllowNegative)
        return "<span>" + _("unknown") + "</span>";

    QString Str = BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nValue);
    if (AllowNegative && nValue > 0)
        Str = '+' + Str;
    return std::string("<span>") + Str.toUtf8().data() + "</span>";
}

static std::string ScriptToString(const CScript& Script, bool Long = false, bool Highlight = false)
{
    if (Script.empty())
        return "unknown";

    CTxDestination Dest;
    if (ExtractDestination(Script, Dest))
    {
        std::string address = EncodeDestination(Dest);
        if (Highlight)
            return "<span class=\"addr\">" + address + "</span>";
        else
            return makeHRef(address);
    }
    else {
        std::string scriptStr = ScriptToAsmStr(Script);
        if (scriptStr.find(std::string("OP_RETURN ")) != std::string::npos) {
            std::string decodedpayload = scriptStr.substr(10);
            std::vector<unsigned char> parsedhexv = ParseHex(decodedpayload);
            std::string parsedhex(parsedhexv.begin(), parsedhexv.end());
            return Long? "<pre><span class='font-weight:bold;'>OP RETURN data: </span>" + parsedhex + "</pre>" : _("Non-standard script");
        } else {
            return Long? "<pre>" + scriptStr + "</pre>" : _("Non-standard script");
        }
    }
}

static std::string TimeToString(uint64_t Time)
{
    QDateTime timestamp;
    timestamp.setTime_t(Time);
    return timestamp.toString("yyyy-MM-dd hh:mm:ss").toUtf8().data();
}

static std::string makeHTMLTableRow(const std::string* pCells, int n)
{
    std::string Result = "<tr>";
    for (int i = 0; i < n; i++)
    {
        Result += "<td class=\"d" + utostr(i) + "\">";
        Result += pCells[i];
        Result += "</td>";
    }
    Result += "</tr>";
    return Result;
}

static const char* table = "<table>";

static std::string makeHTMLTable(const std::string* pCells, int nRows, int nColumns)
{
    std::string Table = table;
    for (int i = 0; i < nRows; i++)
        Table += makeHTMLTableRow(pCells + i*nColumns, nColumns);
    Table += "</table>";
    return Table;
}

static std::string TxToRow(const CTransactionRef& tx, const CScript& Highlight = CScript(), const std::string& Prepend = std::string(), int64_t* pSum = NULL)
{
    std::string InAmounts, InAddresses, OutAmounts, OutAddresses;
    int64_t Delta = 0;
    for (unsigned int j = 0; j < tx->vin.size(); j++)
    {
        if (tx->IsCoinBase())
        {
            InAmounts += ValueToString(tx->GetValueOut());
            InAddresses += "coinbase";
        }
        else
        {
            CTxOut PrevOut = getPrevOut(tx->vin[j].prevout);
            InAmounts += ValueToString(PrevOut.nValue);
            InAddresses += ScriptToString(PrevOut.scriptPubKey, false, PrevOut.scriptPubKey == Highlight).c_str();
            if (PrevOut.scriptPubKey == Highlight)
                Delta -= PrevOut.nValue;
        }
        if (j + 1 != tx->vin.size())
        {
            InAmounts += "<br/>";
            InAddresses += "<br/>";
        }
    }
    for (unsigned int j = 0; j < tx->vout.size(); j++)
    {
        CTxOut Out = tx->vout[j];
        OutAmounts += ValueToString(Out.nValue);
        OutAddresses += ScriptToString(Out.scriptPubKey, false, Out.scriptPubKey == Highlight);
        if (Out.scriptPubKey == Highlight)
            Delta += Out.nValue;
        if (j + 1 != tx->vout.size())
        {
            OutAmounts += "<br/>";
            OutAddresses += "<br/>";
        }
    }

    std::string List[8] =
    {
        Prepend,
        makeHRef(tx->GetHash().GetHex()),
        InAddresses,
        InAmounts,
        OutAddresses,
        OutAmounts,
        "",
        ""
    };

    int n = sizeof(List)/sizeof(std::string) - 2;

    if (!Highlight.empty())
    {
        List[n++] = std::string("<font color=\"") + ((Delta > 0)? "green" : "red") + "\">" + ValueToString(Delta, true) + "</font>";
        *pSum += Delta;
        List[n++] = ValueToString(*pSum);
        return makeHTMLTableRow(List, n);
    }
    return makeHTMLTableRow(List + 1, n - 1);
}

CTxOut getPrevOut(const COutPoint &out)
{
    CTransactionRef tx;
    uint256 hashBlock;
    if (GetTransaction(out.hash, tx, Params().GetConsensus(), hashBlock, true))
        return tx->vout[out.n];
    return CTxOut();
}

void getNextIn(const COutPoint& Out, uint256& Hash, unsigned int& n)
{
    // Hash = 0;
    // n = 0;
    // if (paddressmap)
    //    paddressmap->ReadNextIn(Out, Hash, n);
}

const CBlockIndex* getexplorerBlockIndex(int64_t height)
{
    std::string hex = getexplorerBlockHash(height);
    uint256 hash = uint256S(hex);
    return mapBlockIndex[hash];
}

std::string getexplorerBlockHash(int64_t Height)
{
    std::string genesisblockhash = "e798f3ae4f57adcf25740fe43100d95ec4fd5d43a1568bc89e2b25df89ff6cb0";
    CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    if((Height < 0) || (Height > pindexBest->nHeight)) { return genesisblockhash; }

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    while (pblockindex->nHeight > Height)
        pblockindex = pblockindex->pprev;
    return pblockindex->GetBlockHash().GetHex(); // pblockindex->phashBlock->GetHex();
}

std::string BlockToString(CBlockIndex* pBlock)
{
    if (!pBlock)
        return "";

    CBlock block;
    ReadBlockFromDisk(block, pBlock, Params().GetConsensus());

    int64_t Fees = 0;
    int64_t OutVolume = 0;
    int64_t Reward = 0;

    std::string TxLabels[] = {_("Hash"), _("From"), _("Amount"), _("To"), _("Amount")};

    std::string TxContent = table + makeHTMLTableRow(TxLabels, sizeof(TxLabels)/sizeof(std::string));
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction& tx = *block.vtx[i];
        TxContent += TxToRow(MakeTransactionRef(tx));

        int64_t In = getTxIn(tx);
        int64_t Out = tx.GetValueOut();
        if (tx.IsCoinBase())
            Reward += Out;
        else if (In < 0)
            Fees = -MAX_MONEY;
        else
        {
            Fees += In - Out;
            OutVolume += Out;
        }
    }
    TxContent += "</table>";

    int64_t Generated;
    if (pBlock->nHeight == 0) {
        Generated = OutVolume;
    }
    else {
        const CChainParams& chainParameters = Params();
        Generated = GetBlockSubsidy(pBlock->nHeight - 1, pBlock->nDifficulty, chainParameters.GetConsensus());
    }

    uint256 hash = block.GetHash();
    std::vector<uint8_t> vHash(hash.begin(), hash.end());
    PoW pow(&vHash, block.nShift, &block.nAdd, block.nDifficulty);

    std::vector<uint8_t> vStart, vEnd;
    pow.get_gap(&vStart, &vEnd);

    std::vector<uint8_t> vDifficulty(block.nAdd.begin(), block.nAdd.end());

    // insert 0 a the begining to avoid sig problems
    vDifficulty.push_back(0);
    vStart.push_back(0);
    vEnd.push_back(0);

    CBigNum bnTarget, bnStart, bnEnd;
    bnTarget.setvch(vDifficulty);
    bnStart.setvch(vStart);
    bnEnd.setvch(vEnd);

    CTxDestination address;
    std::string BlockContentCells[] =
    {
        _("Height"),      itostr(pBlock->nHeight),
        _("Size"),        itostr(GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)),
        _("Number of Transactions"), itostr(block.vtx.size()),
        _("Value Out"),   ValueToString(OutVolume),
        _("Fees"),        ValueToString(Fees),
        _("Miner Address"), (block.vtx[0]->vout.size() > 1)? "multiple" : ExtractDestination(block.vtx[0]->vout[0].scriptPubKey, address) ? EncodeDestination(address) : "invalid",
        _("Generated"),   ValueToString(Generated),
        _("Timestamp"),   TimeToString(block.nTime),
        _("Difficulty"),  strprintf("%.4f", GetDifficulty(pBlock)),
        _("Difficulty"),  utostr(block.nDifficulty),
        _("Nonce"),       utostr(block.nNonce),
        _("Shift"),       utostr((uint64_t)block.nShift),
        _("Adder"),       bnTarget.ToString(),
        _("Primedigits"), itostr(bnStart.ToString().length()),
        _("Gapstart"),    bnStart.ToString(),
        _("Gapend"),      bnEnd.ToString(),
        _("Gaplen"),      utostr(pow.gap_len()),
        _("Merit"),       utostr(utils->get_readable_difficulty(pow.merit())),
        _("Version"),     itostr(block.nVersion),
        _("Hash"),        "<pre>" + block.GetHash().GetHex() + "</pre>",
        _("Merkle Root"), "<pre>" + block.hashMerkleRoot.GetHex() + "</pre>",
    };

    std::string BlockContent = makeHTMLTable(BlockContentCells, sizeof(BlockContentCells)/(2*sizeof(std::string)), 2);

    std::string Content;
    Content += "<h4><a class=\"nav\" href=";
    Content += itostr(pBlock->nHeight - 1);
    Content += ">◄&nbsp;</a>";
    Content += _("Block");
    Content += " ";
    Content += itostr(pBlock->nHeight);
    Content += "<a class=\"nav\" href=";
    Content += itostr(pBlock->nHeight + 1);
    Content += ">&nbsp;►</a></h4>";
    Content += BlockContent;
    Content += "</br>";
    Content += "<h4>" + _("Transactions") + "</h4>";
    Content += TxContent;

    return Content;
}

std::string TxToString(uint256 BlockHash, const CTransaction& tx)
{
    int64_t Input = 0;
    int64_t Output = tx.GetValueOut();

    std::string InputsContentCells[] = {_("#"), _("Taken from"),  _("Address"), _("Amount")};
    std::string InputsContent  = makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells)/sizeof(std::string));
    std::string OutputsContentCells[] = {_("#"), _("Redeemed in"), _("Address"), _("Amount")};
    std::string OutputsContent = makeHTMLTableRow(OutputsContentCells, sizeof(OutputsContentCells)/sizeof(std::string));

    if (tx.IsCoinBase())
    {
        std::string InputsContentCells[] =
        {
            "0",
            "coinbase",
            "-",
            ValueToString(Output)
        };
        InputsContent += makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells)/sizeof(std::string));
    }
    else for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint Out = tx.vin[i].prevout;
        CTxOut PrevOut = getPrevOut(tx.vin[i].prevout);
        if (PrevOut.nValue < 0)
            Input = -MAX_MONEY;
        else
            Input += PrevOut.nValue;
        std::string InputsContentCells[] =
        {
            itostr(i),
            "<span>" + makeHRef(Out.hash.GetHex()) + ":" + itostr(Out.n) + "</span>",
            ScriptToString(PrevOut.scriptPubKey, true),
            ValueToString(PrevOut.nValue)
        };
        InputsContent += makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells)/sizeof(std::string));
    }

    uint256 TxHash = tx.GetHash();
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& Out = tx.vout[i];
        uint256 HashNext = uint256S("0");
        unsigned int nNext = 0;
        bool fAddrIndex = false;
        getNextIn(COutPoint(TxHash, i), HashNext, nNext);
        std::string OutputsContentCells[] =
        {
            itostr(i),
            (HashNext == uint256S("0"))? (fAddrIndex? _("no") : _("unknown")) : "<span>" + makeHRef(HashNext.GetHex()) + ":" + itostr(nNext) + "</span>",
            ScriptToString(Out.scriptPubKey, true),
            ValueToString(Out.nValue)
        };
        OutputsContent += makeHTMLTableRow(OutputsContentCells, sizeof(OutputsContentCells)/sizeof(std::string));
    }

    InputsContent  = table + InputsContent  + "</table>";
    OutputsContent = table + OutputsContent + "</table>";

    std::string Hash = TxHash.GetHex();

    std::string Labels[] =
    {
        _("In Block"),  "",
        _("Size"),      itostr(GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)),
        _("Input"),     tx.IsCoinBase()? "-" : ValueToString(Input),
        _("Output"),    ValueToString(Output),
        _("Fees"),      tx.IsCoinBase()? "-" : ValueToString(Input - Output),
        _("Timestamp"), "",
        _("Hash"),      "<pre>" + Hash + "</pre>",
    };

    // std::map<uint256, CBlockIndex*>::iterator iter = mapBlockIndex.find(BlockHash);
    BlockMap::iterator iter = mapBlockIndex.find(BlockHash);
    if (iter != mapBlockIndex.end())
    {
        CBlockIndex* pIndex = iter->second;
        Labels[0*2 + 1] = makeHRef(itostr(pIndex->nHeight));
        Labels[5*2 + 1] = TimeToString(pIndex->nTime);
    }

    std::string Content;
    Content += "<h4>" + _("Transaction") + "&nbsp;<span>" + Hash + "</span></h4>";
    Content += makeHTMLTable(Labels, sizeof(Labels)/(2*sizeof(std::string)), 2);
    Content += "</br>";
    Content += "<h5>" + _("Inputs") + "</h5>";
    Content += InputsContent;
    Content += "</br>";
    Content += "<h5>" + _("Outputs") + "</h5>";
    Content += OutputsContent;

    return Content;
}

std::string AddressToString(const CTxDestination& address)
{
    std::string TxLabels[] =
    {
        _("Date"),
        _("Hash"),
        _("From"),
        _("Amount"),
        _("To"),
        _("Amount"),
        _("Delta"),
        _("Balance")
    };
    std::string TxContent = table + makeHTMLTableRow(TxLabels, sizeof(TxLabels)/sizeof(std::string));

    std::set<COutPoint> PrevOuts;

    CScript script = GetScriptForDestination(address);
    CScriptID scid(script);

    /*
    bool fAddrIndex = false;
    CScript AddressScript;
    AddressScript.SetDestination(Address.Get());
    int64_t Sum = 0;
    */
    bool fAddrIndex = false;
    if (!fAddrIndex)
        return ""; // it will take too long to find transactions by address
    else
    {
        std::vector<CDiskTxPos> Txs;
        /*
        paddressmap->GetTxs(Txs, AddressScript.GetID());
        BOOST_FOREACH (const CDiskTxPos& pos, Txs)
        {
            CTransaction tx;
            CBlock block;
            uint256 bhash = block.GetHash();
            GetTransaction(pos.nTxOffset, tx, bhash);
            std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
            if (mi == mapBlockIndex.end())
                continue;
            CBlockIndex* pindex = (*mi).second;
            if (!pindex || !chainActive.Contains(pindex))
                continue;
            std::string Prepend = "<a href=\"" + itostr(pindex->nHeight) + "\">" + TimeToString(pindex->nTime) + "</a>";
            TxContent += TxToRow(tx, AddressScript, Prepend, &Sum);
        }
        */
    }
    TxContent += "</table>";

    const CKeyID *keyid = boost::get<CKeyID>(&address);
    std::string Content;
    Content += "<h1>" + _("Transactions to/from") + "&nbsp;<span class=\"mono\">" + keyid->ToString() + "</span></h1>";
    Content += TxContent;
    return Content;
}

BlockExplorer::BlockExplorer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BlockExplorer),
    m_NeverShown(true),
    m_HistoryIndex(0)
{
    ui->setupUi(this);

    connect(ui->pushSearch, SIGNAL(released()), this, SLOT(onSearch()));
    connect(ui->content, SIGNAL(linkActivated(const QString&)), this, SLOT(goTo(const QString&)));
    connect(ui->back, SIGNAL(released()), this, SLOT(back()));
    connect(ui->forward, SIGNAL(released()), this, SLOT(forward()));
}

BlockExplorer::~BlockExplorer()
{
    delete ui;
}

void BlockExplorer::keyPressEvent(QKeyEvent *event)
{
    switch ((Qt::Key)event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        onSearch();
        return;

    default:
        return QMainWindow::keyPressEvent(event);
    }
}

void BlockExplorer::showEvent(QShowEvent*)
{
    if (m_NeverShown)
    {
        m_NeverShown = false;
        
        CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];

        setBlock(pindexBest);
        QString text = QString("%1").arg(pindexBest->nHeight);
        ui->searchBox->setText(text);
        m_History.push_back(text);
        updateNavButtons();

        if (!gArgs.GetBoolArg("-txindex", false))
        {
            QString Warning = tr("Not all transactions will be shown. To view all transactions you need to set txindex=1 in the configuration file (vcore.conf).");
            QMessageBox::warning(this, "Gapcoin Blockchain Explorer", Warning, QMessageBox::Ok);
        }
    }
}

bool BlockExplorer::switchTo(const QString& query)
{

    bool IsOk;
    int64_t AsInt = query.toInt(&IsOk);
    // If query is integer, get hash from height
    if (IsOk && AsInt >= 0 && AsInt <= chainActive.Tip()->nHeight)
    {
        std::string hex = getexplorerBlockHash(AsInt);
        uint256 hash = uint256S(hex);
        CBlockIndex* pIndex = mapBlockIndex[hash];
        if (pIndex)
        {
            setBlock(pIndex);
            return true;
        }
    }

    // If the query is not an integer, assume it is a block hash
    uint256 hash = uint256S(query.toUtf8().constData());

    // std::map<uint256, CBlockIndex*>::iterator iter = mapBlockIndex.find(hash);
    BlockMap::iterator iter = mapBlockIndex.find(hash);
    if (iter != mapBlockIndex.end())
    {
        setBlock(iter->second);
        return true;
    }

    // If the query is neither an integer nor a block hash, assume a transaction hash
    CTransactionRef tx;
    uint256 hashBlock = uint256S("0");
    if (GetTransaction(hash, tx, Params().GetConsensus(), hashBlock, true))
    {
        setContent(TxToString(hashBlock, *tx));
        return true;
    }

    // If the query is not an integer, nor a block hash, nor a transaction hash, assume an address
    CTxDestination dest = DecodeDestination(query.toUtf8().constData());
    if (IsValidDestination(dest))
    {
        // std::string Content = EncodeDestination(dest);
        std::string Content = AddressToString(dest);
        if (Content.empty())
            return false;
        setContent(Content);
        return true;
    }

    return false;
}

void BlockExplorer::goTo(const QString& query)
{
    if (switchTo(query))
    {
        ui->searchBox->setText(query);
        while (m_History.size() > m_HistoryIndex + 1)
            m_History.pop_back();
        m_History.push_back(query);
        m_HistoryIndex = m_History.size() - 1;
        updateNavButtons();
    }
}

void BlockExplorer::onSearch()
{
    goTo(ui->searchBox->text());
}

void BlockExplorer::setBlock(CBlockIndex* pBlock)
{
    setContent(BlockToString(pBlock));
}

void BlockExplorer::setContent(const std::string& Content)
{
    QString CSS = "body {font-size:12px; background-color: #C8E5E2; color:#444;}\n a, span { font-family: monospace; }\n span.addr {color:#13BE5D; font-weight: bold;}\n table tr td {padding: 3px; border: none; background-color: #A1CDC8;}\n td.d0 {font-weight: bold; color:#f8f8f8;}\n h2, h3 { white-space:nowrap; color:#1B7884;}\n a { text-decoration:none; }\n a.nav {color:green;}\n";
    QString FullContent = "<html><head><style type=\"text/css\">" + CSS + "</style></head>" + "<body>" + Content.c_str() + "</body></html>";
    // printf(FullContent.toUtf8());
    ui->content->setText(FullContent);
}

void BlockExplorer::back()
{
    int NewIndex = m_HistoryIndex - 1;
    if (0 <= NewIndex && NewIndex < m_History.size())
    {
         m_HistoryIndex = NewIndex;
         ui->searchBox->setText(m_History[NewIndex]);
         switchTo(m_History[NewIndex]);
         updateNavButtons();
    }
}

void BlockExplorer::forward()
{
    int NewIndex = m_HistoryIndex + 1;
    if (0 <= NewIndex && NewIndex < m_History.size())
    {
         m_HistoryIndex = NewIndex;
         ui->searchBox->setText(m_History[NewIndex]);
         switchTo(m_History[NewIndex]);
         updateNavButtons();
    }
}

void BlockExplorer::updateNavButtons()
{
    ui->back->setEnabled(m_HistoryIndex - 1 >= 0);
    ui->forward->setEnabled(m_HistoryIndex + 1 < m_History.size());
}
