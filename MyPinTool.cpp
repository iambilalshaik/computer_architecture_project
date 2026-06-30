//this is the project of bilal and arun gopi
#include "pin.H"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>

using namespace std;

/* ============================================================
   FILES
   ============================================================ */

ofstream traceFile;
ofstream depFile;
ofstream ilpFile;
ofstream graphFile;

/* ============================================================
   GLOBALS
   ============================================================ */

UINT64 insCount = 0;

/* ============================================================
   DEP TYPES
   ============================================================ */

enum DepType
{
    RAW_REG,
    WAR_REG,
    WAW_REG,

    RAW_MEM,
    WAR_MEM,
    WAW_MEM,

    CONTROL_DEP,

    FLAG_DEP
};

/* ============================================================
   EDGE
   ============================================================ */

struct Edge
{
    UINT64 from;
    UINT64 to;

    DepType type;

    string resource;
};

/* ============================================================
   INST INFO
   ============================================================ */

struct InstInfo
{
    UINT64 seq;

    ADDRINT pc;

    string disasm;

    vector<REG> readRegs;
    vector<REG> writeRegs;

    bool memRead;
    bool memWrite;

    ADDRINT readAddr;
    ADDRINT writeAddr;

    bool isBranch;

    bool readsFlags;
    bool writesFlags;
};

/* ============================================================
   GLOBAL STORAGE
   ============================================================ */

vector<InstInfo> traceVec;

vector<Edge> depEdges;

/* ============================================================
   HELPERS
   ============================================================ */

REG NormalizeReg(REG r)
{
    return REG_FullRegName(r);
}

string DepToString(DepType t)
{
    switch (t)
    {
    case RAW_REG:
        return "RAW_REG";

    case WAR_REG:
        return "WAR_REG";

    case WAW_REG:
        return "WAW_REG";

    case RAW_MEM:
        return "RAW_MEM";

    case WAR_MEM:
        return "WAR_MEM";

    case WAW_MEM:
        return "WAW_MEM";

    case CONTROL_DEP:
        return "CONTROL_DEP";

    case FLAG_DEP:
        return "FLAG_DEP";
    }

    return "UNKNOWN";
}

/* ============================================================
   ADD EDGE
   ============================================================ */

void AddDependency(UINT64 from,
                   UINT64 to,
                   DepType type,
                   string resource)
{
    if (from == to)
        return;

    Edge e;

    e.from = from;
    e.to = to;
    e.type = type;
    e.resource = resource;

    depEdges.push_back(e);
}

/* ============================================================
   TRACE
   ============================================================ */

VOID PrintInst(ADDRINT ip,
               UINT32 size,
               string *disasm,

               BOOL isRead,
               BOOL isWrite,

               ADDRINT readAddr,
               ADDRINT writeAddr,

               vector<REG> *rRegs,
               vector<REG> *wRegs,

               BOOL isBranch,

               BOOL readsFlags,
               BOOL writesFlags)
{
    insCount++;

    traceFile << "Instruction Count: "
              << dec
              << insCount
              << " ";

    traceFile << "PC: 0x"
              << hex
              << ip
              << " ";

    traceFile << "ASM: "
              << *disasm
              << " ";

    if (isRead)
    {
        traceFile << "[READ] 0x"
                  << hex
                  << readAddr
                  << " ";
    }

    if (isWrite)
    {
        traceFile << "[WRITE] 0x"
                  << hex
                  << writeAddr
                  << " ";
    }

    traceFile << endl;

    InstInfo info;

    info.seq = insCount;

    info.pc = ip;

    info.disasm = *disasm;

    info.readRegs = *rRegs;
    info.writeRegs = *wRegs;

    info.memRead = isRead;
    info.memWrite = isWrite;

    info.readAddr = readAddr;
    info.writeAddr = writeAddr;

    info.isBranch = isBranch;

    info.readsFlags = readsFlags;
    info.writesFlags = writesFlags;

    traceVec.push_back(info);
}

/* ============================================================
   INSTRUMENTATION
   ============================================================ */

VOID Instruction(INS ins, VOID *v)
{
    string *disasm =
        new string(INS_Disassemble(ins));

    vector<REG> *readRegs =
        new vector<REG>();

    vector<REG> *writeRegs =
        new vector<REG>();

    UINT32 maxR = INS_MaxNumRRegs(ins);

    for (UINT32 i = 0; i < maxR; i++)
    {
        REG r = INS_RegR(ins, i);

        if (r != REG_INVALID())
        {
            readRegs->push_back(r);
        }
    }

    UINT32 maxW = INS_MaxNumWRegs(ins);

    for (UINT32 i = 0; i < maxW; i++)
    {
        REG w = INS_RegW(ins, i);

        if (w != REG_INVALID())
        {
            writeRegs->push_back(w);
        }
    }

    BOOL readsFlags = FALSE;
    BOOL writesFlags = FALSE;

    for (size_t i = 0; i < readRegs->size(); i++)
    {
        if (NormalizeReg((*readRegs)[i]) == REG_RFLAGS)
        {
            readsFlags = TRUE;
        }
    }

    for (size_t i = 0; i < writeRegs->size(); i++)
    {
        if (NormalizeReg((*writeRegs)[i]) == REG_RFLAGS)
        {
            writesFlags = TRUE;
        }
    }

    BOOL isBranch = INS_IsBranch(ins);

    if (INS_IsMemoryRead(ins) &&
        INS_IsMemoryWrite(ins))
    {
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       (AFUNPTR)PrintInst,

                       IARG_INST_PTR,
                       IARG_UINT32,
                       INS_Size(ins),

                       IARG_PTR,
                       disasm,

                       IARG_BOOL,
                       TRUE,

                       IARG_BOOL,
                       TRUE,

                       IARG_MEMORYREAD_EA,
                       IARG_MEMORYWRITE_EA,

                       IARG_PTR,
                       readRegs,

                       IARG_PTR,
                       writeRegs,

                       IARG_BOOL,
                       isBranch,

                       IARG_BOOL,
                       readsFlags,

                       IARG_BOOL,
                       writesFlags,

                       IARG_END);
    }
    else if (INS_IsMemoryRead(ins))
    {
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       (AFUNPTR)PrintInst,

                       IARG_INST_PTR,
                       IARG_UINT32,
                       INS_Size(ins),

                       IARG_PTR,
                       disasm,

                       IARG_BOOL,
                       TRUE,

                       IARG_BOOL,
                       FALSE,

                       IARG_MEMORYREAD_EA,
                       IARG_ADDRINT,
                       0,

                       IARG_PTR,
                       readRegs,

                       IARG_PTR,
                       writeRegs,

                       IARG_BOOL,
                       isBranch,

                       IARG_BOOL,
                       readsFlags,

                       IARG_BOOL,
                       writesFlags,

                       IARG_END);
    }
    else if (INS_IsMemoryWrite(ins))
    {
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       (AFUNPTR)PrintInst,

                       IARG_INST_PTR,
                       IARG_UINT32,
                       INS_Size(ins),

                       IARG_PTR,
                       disasm,

                       IARG_BOOL,
                       FALSE,

                       IARG_BOOL,
                       TRUE,

                       IARG_ADDRINT,
                       0,

                       IARG_MEMORYWRITE_EA,

                       IARG_PTR,
                       readRegs,

                       IARG_PTR,
                       writeRegs,

                       IARG_BOOL,
                       isBranch,

                       IARG_BOOL,
                       readsFlags,

                       IARG_BOOL,
                       writesFlags,

                       IARG_END);
    }
    else
    {
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       (AFUNPTR)PrintInst,

                       IARG_INST_PTR,
                       IARG_UINT32,
                       INS_Size(ins),

                       IARG_PTR,
                       disasm,

                       IARG_BOOL,
                       FALSE,

                       IARG_BOOL,
                       FALSE,

                       IARG_ADDRINT,
                       0,

                       IARG_ADDRINT,
                       0,

                       IARG_PTR,
                       readRegs,

                       IARG_PTR,
                       writeRegs,

                       IARG_BOOL,
                       isBranch,

                       IARG_BOOL,
                       readsFlags,

                       IARG_BOOL,
                       writesFlags,

                       IARG_END);
    }
}

/* ============================================================
   DEPENDENCIES
   ============================================================ */

void DetectDependencies()
{
    map<int, UINT64> lastWriterReg;
    map<int, UINT64> lastReaderReg;

    map<ADDRINT, UINT64> lastWriterMem;
    map<ADDRINT, UINT64> lastReaderMem;

    UINT64 lastControl = 0;

    UINT64 lastFlagWriter = 0;

    for (size_t i = 0; i < traceVec.size(); i++)
    {
        InstInfo &curr = traceVec[i];

        /* ====================================================
           RAW REG
           ==================================================== */

        for (size_t j = 0;
             j < curr.readRegs.size();
             j++)
        {
            int r =
                (int)NormalizeReg(curr.readRegs[j]);

            if (lastWriterReg.find(r) != lastWriterReg.end())
            {
                AddDependency(lastWriterReg[r],
                              curr.seq,
                              RAW_REG,
                              REG_StringShort((REG)r));
            }

            lastReaderReg[r] = curr.seq;
        }

        /* ====================================================
           WAR REG
           ==================================================== */

        for (size_t j = 0;
             j < curr.writeRegs.size();
             j++)
        {
            int w =
                (int)NormalizeReg(curr.writeRegs[j]);

            if (lastReaderReg.find(w) != lastReaderReg.end())
            {
                AddDependency(lastReaderReg[w],
                              curr.seq,
                              WAR_REG,
                              REG_StringShort((REG)w));
            }
        }

        /* ====================================================
           WAW REG
           ==================================================== */

        for (size_t j = 0;
             j < curr.writeRegs.size();
             j++)
        {
            int w =
                (int)NormalizeReg(curr.writeRegs[j]);

            if (lastWriterReg.find(w) != lastWriterReg.end())
            {
                AddDependency(lastWriterReg[w],
                              curr.seq,
                              WAW_REG,
                              REG_StringShort((REG)w));
            }

            lastWriterReg[w] = curr.seq;
        }

        /* ====================================================
           RAW MEM
           ==================================================== */

        if (curr.memRead)
        {
            if (lastWriterMem.find(curr.readAddr) != lastWriterMem.end())
            {
                AddDependency(lastWriterMem[curr.readAddr],
                              curr.seq,
                              RAW_MEM,
                              "MEM[0x" +
                                  LEVEL_BASE::StringFromAddrint(curr.readAddr) + "]");
            }

            lastReaderMem[curr.readAddr] =
                curr.seq;
        }

        /* ====================================================
           WAR MEM
           ==================================================== */

        if (curr.memWrite)
        {
            if (lastReaderMem.find(curr.writeAddr) != lastReaderMem.end())
            {
                AddDependency(lastReaderMem[curr.writeAddr],
                              curr.seq,
                              WAR_MEM,
                              "MEM[0x" +
                                  LEVEL_BASE::StringFromAddrint(curr.writeAddr) + "]");
            }
        }

        /* ====================================================
           WAW MEM
           ==================================================== */

        if (curr.memWrite)
        {
            if (lastWriterMem.find(curr.writeAddr) != lastWriterMem.end())
            {
                AddDependency(lastWriterMem[curr.writeAddr],
                              curr.seq,
                              WAW_MEM,
                              "MEM[0x" +
                                  LEVEL_BASE::StringFromAddrint(curr.writeAddr) + "]");
            }

            lastWriterMem[curr.writeAddr] =
                curr.seq;
        }

        /* ====================================================
           FLAG DEP
           ==================================================== */

        if (curr.readsFlags &&
            lastFlagWriter != 0)
        {
            AddDependency(lastFlagWriter,
                          curr.seq,
                          FLAG_DEP,
                          "RFLAGS");
        }

        if (curr.writesFlags)
        {
            lastFlagWriter = curr.seq;
        }

        /* ====================================================
           CONTROL DEP
           ==================================================== */

        if (lastControl != 0)
        {
            AddDependency(lastControl,
                          curr.seq,
                          CONTROL_DEP,
                          "CONTROL");
        }

        if (curr.isBranch)
        {
            lastControl = curr.seq;
        }
    }

    depFile << "DEPENDENCIES\n\n";

    for (size_t i = 0;
         i < depEdges.size();
         i++)
    {
        depFile
            << depEdges[i].from
            << " -> "
            << depEdges[i].to
            << " : "
            << DepToString(depEdges[i].type)
            << " ("
            << depEdges[i].resource
            << ")"
            << endl;
    }
}

/* ============================================================
   EXPORT GRAPH
   ============================================================ */

void ExportGraph() // to convert graph.file to graph.png //
{
    graphFile << "digraph DAG {\n";

    graphFile << "rankdir=LR;\n";

    for (size_t i = depEdges.size() - 50;
         i < depEdges.size();
         i++)
    {
        graphFile
            << depEdges[i].from
            << " -> "
            << depEdges[i].to
            << " [label=\""
            << DepToString(depEdges[i].type)
            << "\\n"
            << depEdges[i].resource
            << "\"];\n";
    }

    graphFile << "}\n";
}

/* ============================================================
   ILP
   ============================================================ */

void EstimateILP()
{
    UINT64 N = traceVec.size();

    vector<vector<UINT64>> adj(N + 1);

    vector<int> indegree(N + 1, 0);

    for (size_t i = 0;
         i < depEdges.size();
         i++)
    {
        UINT64 u = depEdges[i].from;

        UINT64 v = depEdges[i].to;

        adj[u].push_back(v);

        indegree[v]++;
    }

    queue<UINT64> q;

    vector<int> level(N + 1, 0);

    for (UINT64 i = 1; i <= N; i++)
    {
        if (indegree[i] == 0)
        {
            q.push(i);
        }
    }

    while (!q.empty())
    {
        UINT64 u = q.front();

        q.pop();

        for (size_t i = 0;
             i < adj[u].size();
             i++)
        {
            UINT64 v = adj[u][i];

            if (level[v] < level[u] + 1)
            {
                level[v] = level[u] + 1;
            }

            indegree[v]--;

            if (indegree[v] == 0)
            {
                q.push(v);
            }
        }
    }

    map<int, int> levelCount;

    int maxLevel = 0;

    for (UINT64 i = 1; i <= N; i++)
    {
        levelCount[level[i]]++;

        if (level[i] > maxLevel)
        {
            maxLevel = level[i];
        }
    }

    ilpFile << "ILP RESULTS\n\n";

    int maxParallel = 0;

    for (int i = 0; i <= maxLevel; i++)
    {
        ilpFile
            << "LEVEL "
            << i
            << " : "
            << levelCount[i]
            << " instructions\n";

        if (levelCount[i] > maxParallel)
        {
            maxParallel = levelCount[i];
        }
    }

    double ilp =
        (double)N /
        (double)(maxLevel + 1);

    ilpFile << "\n";

    ilpFile << "TOTAL INSTRUCTIONS : "
            << N
            << endl;

    ilpFile << "CRITICAL PATH : "
            << maxLevel + 1
            << endl;

    ilpFile << "MAX PARALLELISM : "
            << maxParallel
            << endl;
    ilpFile << "AVERAGE ILP : "
            << ilp
            << endl;
}

/* ============================================================
   FINI
   ============================================================ */

VOID Fini(INT32 code, VOID *v)
{
    DetectDependencies();

    ExportGraph();

    EstimateILP();

    traceFile.close();

    depFile.close();

    ilpFile.close();

    graphFile.close();
}

/* ============================================================
   MAIN
   ============================================================ */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
    {
        cerr << "PIN Init Failed\n";

        return 1;
    }

    traceFile.open("full_trace.out");

    depFile.open("dependencies.out");

    ilpFile.open("ilp.out");

    graphFile.open("dag.dot");

    INS_AddInstrumentFunction(
        Instruction,
        0);

    PIN_AddFiniFunction(
        Fini,
        0);

    PIN_StartProgram();

    return 0;
}
