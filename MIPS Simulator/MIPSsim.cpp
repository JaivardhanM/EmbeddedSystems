/*

On my honor, I have neither given nor received unauthorized aid on this assignment 

Title: MIPS Simulator Project
Author: Sai Raghu Vamsi Anumula  
UFID: 49939544
*/
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <queue>

#define BUF_SIZE 10
#define MEM_SIZE 16
#define NREGS 16

using namespace std;

//vector<string> INB;
std::map<int,int> regs;
int memory[16];

enum Opcodes
{
    ADD,
    SUB,
    MUL,
    ST,
};

class mips_instrn
{
public:
    string op;
    Opcodes opcode;
    //registers
    string rdest;
    string rsrc1;
    string rsrc2;
    // Result 
    int result;
    int csrc1;
    int csrc2;
    int getsrc1();
    int getsrc2();
    int id;
};

int mips_instrn::getsrc1()
{
    return regs[stoi(this->rsrc1)];
}

int mips_instrn::getsrc2()
{
    return regs[stoi(this->rsrc2)];
}

// Declare all the buffers used in the program
std::vector<string> INM;
std::queue<mips_instrn> INB;
std::queue<mips_instrn> AIB;
std::queue<mips_instrn> SIB;
std::queue<mips_instrn> PRB;
std::queue<mips_instrn> ADB;
std::queue<mips_instrn> REB;
typedef map<int,int>::const_iterator MapIterator;

// MIPS INSTR format: 
// <Opcode>, <Destination Register>, <First Source Operand>, <Second Source Operand>
// read Instruction
ifstream fp("instructions.txt");
vector<string> extract_args(string inst)
{
    string ele;
    std::vector<string> v;
    inst.pop_back();
    inst.erase(0,1);
    istringstream ss(inst);
    // Extract registers and opcode from the instruction
    while(getline(ss, ele, ','))
    {
        v.push_back(ele);
    }
    return v;
}

/*
Initialize Registers
*/
void init_regs()
{
    ifstream fp("registers.txt");
    // Initialize registers in an ordered map
    string line;
    std::vector<string> rv;
    while(fp >> line)
    {
        if(line[0] == '<')
        {
            rv = extract_args(line);
            rv[0].erase(0,1);
            regs[stoi(rv[0])] = stoi(rv[1]);
        }
        
    }
}

/*
Initialize Memory
*/
void init_memory()
{
    ifstream fp("datamemory.txt");
    // Initialize data memory in an ordered map
    string line;
    std::vector<string> rv;
    int key,val;
    
    int i;
    for(i = 0; i < 16; i++)
        memory[i] = -99;

    while(fp >> line)
    {
        if(line[0] == '<')
        {
            rv = extract_args(line);
            key = stoi(rv[0]);
            val = stoi(rv[1]);
            // Init memory cell
            memory[key] = val;
        }
    }
}

/*
Decode Place
*/
int trans_id =0;
void instr_decode(string inst)
{   
    mips_instrn cur_decode;
    std::vector<string> v = extract_args(inst);
    // Decode the instruction and construct instr object
    cur_decode.op   = v[0];
    cur_decode.rdest = v[1];
    cur_decode.rsrc1 = v[2];
    cur_decode.rsrc2 = v[3];
    cur_decode.id = trans_id;
    if(cur_decode.op == "ST")
    {   
        cur_decode.rsrc1.erase(0,1);
        cur_decode.csrc1 = cur_decode.getsrc1();
    }
    else
    {   
        cur_decode.rsrc1.erase(0,1);
        cur_decode.rsrc2.erase(0,1);
        cur_decode.csrc1 = cur_decode.getsrc1();
        cur_decode.csrc2 = cur_decode.getsrc2();
    }
    // Append Decoded instruction to Instruction Buffer
    trans_id++;
    INB.push(cur_decode);
}

void instr_issue()
{   
    // If buffer is not empty read from the
    // buffer and check the op code
    if(INB.empty())
        return;
    mips_instrn cur_issue = INB.front();
    INB.pop();

    // if operation is store move to SIB
    if(cur_issue.op == "ST")
        SIB.push(cur_issue);
    // Else it is a ALU operation so move it ot AIB
    else
        AIB.push(cur_issue);
}


void alu_opt()
{
    // IF AIB not empty read from the buffer
    if(AIB.empty())
        return;
    mips_instrn cur_alu = AIB.front();
    AIB.pop();

    // Perform ALU operations
    if(cur_alu.op == "MUL")
    {   // MUL1 unit
        cur_alu.result = cur_alu.csrc1*cur_alu.csrc2;
        PRB.push(cur_alu);
    }
    else
    {   // ADD or SUB unit
        if(cur_alu.op == "ADD")
            cur_alu.result = cur_alu.csrc1 + cur_alu.csrc2;
        else
            cur_alu.result = cur_alu.csrc1 - cur_alu.csrc2;    
        REB.push(cur_alu);
    }

}

void alu_mul2()
{   
    // Read from PRB if not empty and perform MUL2
    if(PRB.empty())
        return;
    mips_instrn cur_mul2 = PRB.front();
    PRB.pop();  
    /*
    Do MUL2 operations at this stage and 
    push the result to the result buffer
    */

    REB.push(cur_mul2);
}

void comp_addr()
{   
    // Read from SIB is not empty and compute store address
    if(SIB.empty())
        return;
    mips_instrn cur_sti = SIB.front();
    SIB.pop();
    // compute address
    int addr;
    addr = cur_sti.csrc1 + stoi(cur_sti.rsrc2);
    cur_sti.result =addr;
    // put the addr as the result of the Addr buffer
    ADB.push(cur_sti);
}

void write_op()
{
    //Read from REB if not empty
    if(REB.empty())
        return;
    mips_instrn cur_write = REB.front();
    REB.pop();

    string rd;
    if(cur_write.op != "ST")
    {   
        rd = cur_write.rdest;
        rd.erase(0,1);
        regs[stoi(rd)] = cur_write.result;
    }
}

void store_op()
{
    //Read from ADB if not empty and do store operation
    if(ADB.empty())
        return;
    mips_instrn cur_str = ADB.front();
    ADB.pop();
    // Execute store to data memory instruction
    string rd;
    rd = cur_str.rdest;
    rd.erase(0,1);
    memory[cur_str.result]= regs[stoi(rd)];
}

void load_instructions()
{   
    //Load all the instructions from the file
    // to INM
    string nInst;
    while(fp >> nInst)
    {
        if(nInst[0] == '<')
        {
            INM.push_back(nInst);
        }
        
    }
}

void read_instruction()
{   
    // Read an instruction from INM
    // if not empty
    string nInst;
    if(!INM.empty())
    {
        nInst = INM.front();
        INM.erase(INM.begin());
        // And issue the decode operation on instruction
        instr_decode(nInst);
    }
}
// print contents
ofstream of("simulation.txt");
void print_contents(int id)
{   
    // Print step number of the instrution
    of << "STEP " << id <<":" <<endl;
    //print INM contents
    of << "INM:";
    int i;
    
    for(i =0; i< INM.size() ; i++)
    {   
        if(i ==0)
            of << INM[i] ;
        else
        of << "," << INM[i];
    }
    
    // Print INB
    of << "\nINB:";
    mips_instrn temp;
    for(i=0;i< INB.size();i++)
    {
        temp = INB.front();
        INB.pop();
        INB.push(temp);
        //print temp content
        if(temp.op == "ST")
        {
            if(i == 0)
                of << "<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.rsrc2 << ">"; 
            else
                of << ",<"<< temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.rsrc2 << ">"; 

        }

        else
        {
            if(i == 0)
                of << "<" << temp.op << ","  << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 
            else
                of << ",<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 


        }
        
    }

    //Print AIB
    of << "\nAIB:" ;
    for(i=0;i< AIB.size();i++)
    {
        temp = AIB.front();
        AIB.pop();
        AIB.push(temp);
        //print temp content
        if(i == 0)
            of << "<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 
        else
            of << ",<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 
    }

    //Print SIB
    of << "\nSIB:";
    for(i=0;i < SIB.size();i++)
    {
        temp = SIB.front();
        SIB.pop();
        SIB.push(temp);
        //print temp content
        if(i == 0)
            of << "<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.rsrc2 << ">"; 
        else
            of << ",<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.rsrc2 << ">"; 
    }
    //Print PRB
    of << "\nPRB:";
    for(i=0;i < PRB.size();i++)
    {
        temp = PRB.front();
        PRB.pop();
        PRB.push(temp);
        //print temp content
        if(i == 0)
            of << "<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 
        else
            of << ",<" << temp.op << "," << temp.rdest <<","<< temp.csrc1 <<"," <<  temp.csrc2 << ">"; 
    }
    //Print ADB
    of << "\nADB:";
    for(i=0;i < ADB.size();i++)
    {
        temp = ADB.front();
        ADB.pop();
        ADB.push(temp);
        //print temp content
        if(i == 0)
            of << "<" << temp.rdest <<","<< temp.result << ">"; 
        else
            of << ",<" << temp.rdest <<","<< temp.result << ">"; 
    }
    //Print REB
    of << "\nREB:";
    for(i=0;i < REB.size();i++)
    {
        temp = REB.front();
        REB.pop();
        REB.push(temp);
        //print temp content
        if(i == 0)
            of << "<" << temp.rdest <<","<< temp.result << ">"; 
        else
            of << ",<" << temp.rdest <<","<< temp.result << ">"; 
    }
    //print RGF
    of << "\nRGF:";
    for (MapIterator iter = regs.begin(); iter != regs.end(); iter++)
    {
        if(iter == regs.begin())
            of << "<R" << iter->first  <<","<< iter->second << ">"; 
        else
            of <<",<R" << iter->first  <<","<< iter->second << ">"; 
    }
    //Print DAM
    of << "\nDAM:";
    int first =0;
    for(i =0; i< MEM_SIZE ; i++ )
    {
     if(memory[i] != -99)
     { 
        if(first == 0)
        {
            first =1;
            of << "<" << i <<","<< memory[i] << ">"; 
        }
        else
            of <<",<" << i <<","<< memory[i] << ">";            
     }
    }
    of << endl;


}

int buffers_empty()
{
    if(INM.empty() && INB.empty() && AIB.empty() && SIB.empty()
        && PRB.empty() && ADB.empty() &&REB.empty())
        return 1;
    else
        return 0;
}

int main(int argc,char * argv[])
{   
    // Init Registers
    init_regs();
    // Initi data memory
    init_memory();
    // Load all the instrution to INM
    load_instructions();
    int stat;
    // Start simulation
    int i =0;
    while(!buffers_empty())
    {       
        print_contents(i);
        of << endl;
        write_op();
        store_op();
        alu_mul2();
        alu_opt();
        comp_addr();
        instr_issue();
        read_instruction();
        i++;
    }
    print_contents(i);
    of.close();


    return 0;
}