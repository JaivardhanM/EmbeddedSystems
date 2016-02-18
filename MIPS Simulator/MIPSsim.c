/*************************************************************************
 * FILE: MIPSsim.c
 * Description:	Simulates behaviour of MIPS processor and shows contents 
 * 				of each stage at consecutive clock cycles until all 
 *				instructions are proccessed.
 *
 * File Owner: 		 Jaivardhan Mattapalli
 * UFID:			 53991381
 * Date of Creation: 14th Feb, 2016
 * Created for: 	 CDA 5636 Embedded Systems Project#1
 *
 * HONESTY STATEMENT:  On my honor, I have neither given nor received any  
 *					   unauthorized aid on this assignment
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <malloc.h>

/* Macro definitions */
#define MAX_LINE_SIZE 	20
#define SET 			1
#define CLEAR 			0
#define REB_SIZE		3
#define FINISH_FLAG 	((InstBuff_Curr.Flag==0) && \
						 (ArithBuff_Curr.Flag==0) && (PartialResBuff_Curr.Flag==0)&& \
						 (ResultBuff_Curr[0].Flag==0) && (ResultBuff_Curr[1].Flag==0) && \
						 (StoreBuff_Curr.Flag==0)&&(AddrDataBuff_Curr.Flag==0))

typedef struct 			/*  Structure to hold contents of Instructions  */
{
	char* Inst;
	char* DReg;
	int S1;
	int S2;
	int Flag;
} INB;

typedef struct 			/*  Structure to hold contents of Addr Dat buff  */
{
	char *DestReg;
	int MemAddr;
	int Flag;
}ADB;

typedef struct 			/*  Structure to hold contents of Result Buffer  */
{
	char *DestReg;
	int DatValue;
	int Flag;
}REB;

INB InstBuff_Prev, InstBuff_Curr, ArithBuff_Prev, ArithBuff_Curr, StoreBuff_Prev, StoreBuff_Curr, PartialResBuff_Prev, PartialResBuff_Curr;

ADB AddrDataBuff_Prev, AddrDataBuff_Curr;

REB ResultBuff_Prev[3], ResultBuff_Curr[3];

FILE *Inst_fd, *OutSim_fd;	/* File pointer declarions for input&output */
size_t line_size = MAX_LINE_SIZE;	/* Max Line size for an instruction */
int Registers[16];			/* 		Register array declaration 			*/
int DataMemory[16];			/* 		Data memory array declaration 		*/ 
int NoInstructions_flag = 0;
int Pos = 0;
int REB_WrIndex = 0;
int REB_RdIndex = 0;
char *Instruction_Queue;	/* String pointer to hold Instruction queue */


/* 	************     Function Prototype Declarations 	 ************   */
void Init_Routine();
void Decode();
void Issue();
void Calc_Addr();
void StoreData();
void PrintContents();
void CopyContents();
void Add_Sub_Mul1_Unit();
void Mul2_Unit();
void WriteBack();
void Dwnld_RegContents();
void Dwnld_DataMemContents();
void Load_InstructionQueue();

/* 	*********************	  Start of MAIN 	   ******************* 	*/
int main()
{
	int i = 0;

	Init_Routine();				/* 		Initialize routine 				*/
	fprintf(OutSim_fd,"STEP 0:\n");	/* 		Print Inital States 		*/
	PrintContents();
	
	while(1)			/* 	Loop Until all instructions are proccessed 	*/
	{
		fprintf(OutSim_fd,"\n\nSTEP %d:\n", i+1);
		Decode();				/* 		Decode Next Instruction 		*/

		if(NoInstructions_flag == 1&& i==0)
		{
			break;
		}
		Issue();				/*	 Issue inst to Store/Arith Unit 	*/
		Mul2_Unit();			/* 		2nd stage of Multiply Unit 		*/
		Add_Sub_Mul1_Unit();	/* 			Arithmetic Unit 			*/
		WriteBack();			/* 			Write Back Unit 			*/
		Calc_Addr();			/* Calc Addr from Inst for Store Unit 	*/
		StoreData();			/*    Write data to Main DataMemory 	*/
		PrintContents();		/*		Print States of all buffers		*/
		CopyContents();			/*Copy contents from prev to next stage */
		/*   	If all stages finsihed their jobs, stop iterating  		*/
		if(FINISH_FLAG) 
		{
			break;
		}
		i++; 					/* 		Increment iteration count 		*/ 
	}
	free(Instruction_Queue);	/* Deallocate memory used by instruction queue */
	fclose(Inst_fd);			/* 		Close Input Instruction file 	*/
	fclose(OutSim_fd);			/* 		Close Output Simulation file 	*/

	return;
}

void Init_Routine()
{
	FILE *Reg_fd, *DataMem_fd;

	Load_InstructionQueue();	/* Load all instructions into Instruction Queue */
	Inst_fd = fopen("instructions.txt", "r"); /*  Open Input instruction file   */

	OutSim_fd = fopen("simulation.txt", "w+");/*  Open Output Sim file to write */
	fclose(OutSim_fd);
	OutSim_fd = fopen("simulation.txt", "a");

	Dwnld_RegContents();		/* 		Load registers contents into an array 	*/
	Dwnld_DataMemContents();	/* 	Load Data memory contents into an array		*/
	NoInstructions_flag = 0;	/* Set flag to indicate no more new instructions*/

	return;
}

void Load_InstructionQueue()
{
	char NextInst_m[100];
	int i = 1;					

	Inst_fd = fopen("instructions.txt", "r"); /* open input Instructions fiel */

	/* 	Allocate memory to Inst Queue of one inst size and copy inst into it  */
	if(fscanf(Inst_fd, "%s", NextInst_m))
	{
		Instruction_Queue = (char*)malloc(sizeof(NextInst_m)+1);
		strcpy(Instruction_Queue, NextInst_m);
	}

	/*   Until all instructions are finished append next inst to Instr Queue  */
	while(fscanf(Inst_fd, "%s", NextInst_m)>0)
	{
		if(*NextInst_m != '<')
			continue;
		Instruction_Queue = (char*)realloc(Instruction_Queue,(strlen(Instruction_Queue)+ strlen(NextInst_m))*sizeof(char));
		strcat(Instruction_Queue, NextInst_m);
	}

	fclose(Inst_fd);	/* 	  Close file after all instructions are read 	 */
}

void Dwnld_RegContents()
{
	char* NextReg_m = NULL;
	char* token = NULL;
	int val;
	int Index;
	int i;
	FILE *Reg_fd;

	Reg_fd = fopen("registers.txt", "r");	/*  Open registers input file 	*/

	for(i = 0;i < 16;i++)
	{
		Registers[i] = -1;			/* 	Initalize all registers with -1 	*/
	}

	/* 	loop unitl all registers are read from input file one after other	*/
	while(getline(&NextReg_m, &line_size , Reg_fd)!=-1)
	{
		token = (char*)strtok(NextReg_m,"<,");
		val = atoi(strtok(NULL,">"));
		Index = atoi(strtok(token,"R"));

		Registers[Index] = val;
	}

	fclose(Reg_fd);		/*  Close Reg input file after all regs are read 	*/
	return;
}

void Dwnld_DataMemContents()
{
	char* NextLoc_m = NULL;
	int val;
	int Index;
	int i;
	FILE *DataMem_fd;

	DataMem_fd = fopen("datamemory.txt", "r");	/*  Open DMem input file  	*/

	for(i = 0;i < 16;i++)
	{
		DataMemory[i] = -1;		/* 	Initalize all Memory locations with -1 	*/
	}

	/* 	loop unitl all Mem loc are read from input file one after other		*/
	while(getline(&NextLoc_m, &line_size , DataMem_fd)!=-1)
	{
		Index = atoi((char*)strtok(NextLoc_m,"<,"));
		val = atoi(strtok(NULL,">"));
		DataMemory[Index] = val;
	}

	fclose(DataMem_fd);	/*  Close DatMem input file after all regs are read */

	return;
}

void Decode()
{
	char *NextInst_m = NULL;
	char* token = NULL;
	char *temp1, *temp2;

	/* 	***********  	Read Next instruction from input file  	**********	*/
	if(getline(&NextInst_m, &line_size , Inst_fd)==-1)
	{
		NoInstructions_flag = 1;
		return;
	}
	
	if(*NextInst_m != '<')
	{
		
		return;
	}

	/* 		Extract Opcode, Destination Reg and Source operands from 		*/
	InstBuff_Prev.Inst =(char*)strtok(NextInst_m,"<,");
	InstBuff_Prev.DReg =(char*)strtok(NULL,",");
	temp1 =(char*)strtok(NULL,",");
	temp2 =(char*)strtok(NULL,",>");

	InstBuff_Prev.S1 = Registers[atoi(strtok(temp1,"R"))];
	InstBuff_Prev.S2 = strcmp(InstBuff_Prev.Inst,"ST") ? Registers[atoi(strtok(temp2,"R"))] : atoi(temp2);
	InstBuff_Prev.Flag = SET;
}

void Issue()
{
	
	if (InstBuff_Curr.Flag == CLEAR)
	{
		return;			/* 	 If there is no Inst from Decode unit, exit 	*/
	}

	/* 	if it is a ARITHMATIC instruction, Forward to Arithmatic Buffer 	*/
	if(strcmp(InstBuff_Curr.Inst,"ST"))
	{
		ArithBuff_Prev.Inst = strdup(InstBuff_Curr.Inst);
		ArithBuff_Prev.DReg = strdup(InstBuff_Curr.DReg);
		ArithBuff_Prev.S1 = InstBuff_Curr.S1;
		ArithBuff_Prev.S2 = InstBuff_Curr.S2;

		ArithBuff_Prev.Flag = SET;
		StoreBuff_Prev.Flag = CLEAR;
	}
	else	/* 	  if it is a STORE instruction, Forward to Store Buffer 	*/
	{
		StoreBuff_Prev.Inst = strdup(InstBuff_Curr.Inst);
		StoreBuff_Prev.DReg = strdup(InstBuff_Curr.DReg);
		StoreBuff_Prev.S1 = InstBuff_Curr.S1;
		StoreBuff_Prev.S2 = InstBuff_Curr.S2;

		ArithBuff_Prev.Flag = CLEAR;
		StoreBuff_Prev.Flag = SET;
	}

	InstBuff_Curr.Flag = CLEAR;

	return;
}

void Add_Sub_Mul1_Unit()
{
	if (ArithBuff_Curr.Flag == CLEAR)
	{
		return; 		/* 	 	If there is no instr issued, exit 		*/
	}

	/*   If the instruction is MULTIPLY, copy res into partial Buffer 	*/
	if(!strcmp(ArithBuff_Curr.Inst,"MUL"))
	{
		PartialResBuff_Prev.Inst = strdup(ArithBuff_Curr.Inst);
		PartialResBuff_Prev.DReg = strdup(ArithBuff_Curr.DReg);
		PartialResBuff_Prev.S1 = ArithBuff_Curr.S1;
		PartialResBuff_Prev.S2 = ArithBuff_Curr.S2;

		PartialResBuff_Prev.Flag = SET;
		// ResultBuff_Prev[0].Flag = CLEAR;
	}
	/* If the instruction is ADD/SUB, copy the result into Result Buff */
	else
	{
		ResultBuff_Prev[REB_WrIndex].DestReg = strdup(ArithBuff_Curr.DReg);
		ResultBuff_Prev[REB_WrIndex].DatValue = strcmp(ArithBuff_Curr.Inst,"ADD")?ArithBuff_Curr.S1-ArithBuff_Curr.S2:ArithBuff_Curr.S1+ArithBuff_Curr.S2;

		ResultBuff_Prev[REB_WrIndex].Flag = SET;
		REB_WrIndex = (REB_WrIndex+1)%REB_SIZE;
}
	ArithBuff_Curr.Flag = CLEAR;
	return;
}

void Mul2_Unit()
{
	if (PartialResBuff_Curr.Flag == CLEAR)
	{
		return;		/* 	 If there is no partial res in buffer, exit 	*/
	}

	/* 	Compute the result and copy Dest and Result into Result Buffer 	*/
	ResultBuff_Prev[REB_WrIndex].DestReg = strdup(PartialResBuff_Curr.DReg);
	ResultBuff_Prev[REB_WrIndex].DatValue = PartialResBuff_Curr.S1*PartialResBuff_Curr.S2;

	ResultBuff_Prev[REB_WrIndex].Flag = SET;
	REB_WrIndex = (REB_WrIndex+1)%REB_SIZE;
	PartialResBuff_Curr.Flag = CLEAR;
	return;
}

void WriteBack()
{
	if(ResultBuff_Curr[REB_RdIndex].Flag == CLEAR /*&& ResultBuff_Curr[1].Flag == CLEAR*/)
	{
		return;		/* if there are no entries in result buffer, exit 	*/
	}
	/* If there's an entry frm arith unit in result buff, write to Reg 	*/
		Registers[atoi(strtok(ResultBuff_Curr[REB_RdIndex].DestReg,"R"))] = ResultBuff_Curr[REB_RdIndex].DatValue;
		ResultBuff_Curr[REB_RdIndex].Flag = CLEAR;
		REB_RdIndex = (REB_RdIndex+1)%REB_SIZE;

	return;
}

void Calc_Addr()
{
	if (StoreBuff_Curr.Flag == CLEAR)
	{
		return;
	}

	AddrDataBuff_Prev.DestReg = strdup(StoreBuff_Curr.DReg);
	AddrDataBuff_Prev.MemAddr = StoreBuff_Curr.S1+StoreBuff_Curr.S2;
	AddrDataBuff_Prev.Flag = SET;
	StoreBuff_Curr.Flag = CLEAR;

	return;
}

void StoreData()
{
	if (AddrDataBuff_Curr.Flag == CLEAR)
	{
		return;
	}
	DataMemory[AddrDataBuff_Curr.MemAddr] = Registers[atoi(strtok(AddrDataBuff_Prev.DestReg, "R"))];
	AddrDataBuff_Curr.Flag = CLEAR;

	return;
}

void PrintContents()
{
	int i = 0;
	int j = 0;
	int FirstOccur = 0;
	int pos_m = 0;
	int NOP;
	fpos_t pos;


	/* 	  Print remaining instructions at this step into output file 	*/
	fprintf(OutSim_fd,"INM:");
	for(i=0; i<strlen(Instruction_Queue+Pos);i++)
	{
		fprintf(OutSim_fd,"%c", *(Instruction_Queue+Pos+i));
		if(*(Instruction_Queue+Pos+i) == '>')
		{
			if(FirstOccur == 0)
			{
				FirstOccur = 1;
				pos_m = i+1;
			}
			if(*(Instruction_Queue+Pos+i+1) == '<')
				fprintf(OutSim_fd, ",");
		}
		
	}
	Pos += pos_m;  /* Note the start point of next instr in instr Queue */


	fprintf(OutSim_fd,"\nINB:"); /* 	Print Contents of Instr buffer 	*/
	InstBuff_Prev.Flag? fprintf(OutSim_fd,"<%s,%s,%d,%d>\n",InstBuff_Prev.Inst,InstBuff_Prev.DReg, InstBuff_Prev.S1, InstBuff_Prev.S2): fprintf(OutSim_fd,"\n");

	fprintf(OutSim_fd,"AIB:");	/*  Print Contents of Arith Instr Buff  */
	ArithBuff_Prev.Flag? fprintf(OutSim_fd,"<%s,%s,%d,%d>\n",ArithBuff_Prev.Inst,ArithBuff_Prev.DReg, ArithBuff_Prev.S1, ArithBuff_Prev.S2): fprintf(OutSim_fd,"\n");

	fprintf(OutSim_fd,"SIB:");  /*  Print Contents of Store Instr Buff  */
	StoreBuff_Prev.Flag? fprintf(OutSim_fd,"<%s,%s,%d,%d>\n",StoreBuff_Prev.Inst,StoreBuff_Prev.DReg, StoreBuff_Prev.S1, StoreBuff_Prev.S2): fprintf(OutSim_fd,"\n");

	fprintf(OutSim_fd,"PRB:");	/*  Print Contents of Partial Res Buff  */
	PartialResBuff_Prev.Flag? fprintf(OutSim_fd,"<%s,%s,%d,%d>\n",PartialResBuff_Prev.Inst,PartialResBuff_Prev.DReg, PartialResBuff_Prev.S1, PartialResBuff_Prev.S2): fprintf(OutSim_fd,"\n");

	fprintf(OutSim_fd,"ADB:");	/*  Print Contents of Addr Data Buff  	*/
	AddrDataBuff_Prev.Flag? fprintf(OutSim_fd,"<%s,%d>\n",AddrDataBuff_Prev.DestReg, AddrDataBuff_Prev.MemAddr): fprintf(OutSim_fd,"\n");

	fprintf(OutSim_fd,"REB:");	/* Prnt ResBuff Cntnts seperated by ','	*/
	FirstOccur = 0;
	for (j = 0; j<REB_SIZE; j++)
	{
		i = (j)%REB_SIZE;
		if (ResultBuff_Curr[i].Flag!=0||ResultBuff_Prev[i].Flag!=0)
		{
			if(FirstOccur == 0)
			{
				fprintf(OutSim_fd,"<%s,%d>", ResultBuff_Prev[i].DestReg, ResultBuff_Prev[i].DatValue);
				FirstOccur++;
			}
			else
			{
				fprintf(OutSim_fd,",<%s,%d>", ResultBuff_Prev[i].DestReg, ResultBuff_Prev[i].DatValue);
			}
		}
		
	}
	fprintf(OutSim_fd,"\nRGF:");/* Print Reg contents seperated by ',' 	*/
	FirstOccur = 0;
	for(i = 0; i<16; i++)
	{
		if(Registers[i]!=-1)
		{
			if(FirstOccur!=0)
			{
				fprintf(OutSim_fd,","); /* Insert ',' between registers */
			}
			FirstOccur = 1;
			fprintf(OutSim_fd,"<R%d,%d>",i,Registers[i]);
		}
	}
	FirstOccur = 0;
	fprintf(OutSim_fd,"\nDAM:");/* Print Mem contents seperated by ',' 	*/
	for(i = 0; i<16; i++)
	{
		if(DataMemory[i]!=-1)
		{
			if(FirstOccur!=0)
			{
				fprintf(OutSim_fd,",");/* Insert ',' between Dmem Locs */
			}
			FirstOccur = 1;
			fprintf(OutSim_fd,"<%d,%d>",i,DataMemory[i]);
		}
	}
}

void CopyContents()
{
	int i =0;
	
	/* 	 Copy Contents of each buffer from one stage to next state 	 */
	InstBuff_Curr.Inst = strdup(InstBuff_Prev.Inst);
	InstBuff_Curr.DReg = strdup(InstBuff_Prev.DReg);
	InstBuff_Curr.S1 = InstBuff_Prev.S1;
	InstBuff_Curr.S2 = InstBuff_Prev.S2;
	InstBuff_Curr.Flag = InstBuff_Prev.Flag;
	InstBuff_Prev.Flag = CLEAR;

	if(StoreBuff_Prev.Flag)
	{
		StoreBuff_Curr.DReg = strdup(StoreBuff_Prev.DReg);
		StoreBuff_Curr.S1 = StoreBuff_Prev.S1;
		StoreBuff_Curr.S2 = StoreBuff_Prev.S2;
		StoreBuff_Curr.Flag = StoreBuff_Prev.Flag;
		StoreBuff_Prev.Flag = CLEAR;
	}

	if(AddrDataBuff_Prev.Flag)
	{
		AddrDataBuff_Curr.DestReg = strdup(AddrDataBuff_Prev.DestReg);
		AddrDataBuff_Curr.MemAddr = AddrDataBuff_Prev.MemAddr;
		AddrDataBuff_Curr.Flag = AddrDataBuff_Prev.Flag;
		AddrDataBuff_Prev.Flag = CLEAR;
	}

	if(ArithBuff_Prev.Flag)
	{
		ArithBuff_Curr.Inst = strdup(ArithBuff_Prev.Inst);
		ArithBuff_Curr.DReg = strdup(ArithBuff_Prev.DReg);
		ArithBuff_Curr.S1 = ArithBuff_Prev.S1;
		ArithBuff_Curr.S2 = ArithBuff_Prev.S2;
		ArithBuff_Curr.Flag = ArithBuff_Prev.Flag;
		ArithBuff_Prev.Flag = CLEAR;
	}

	if(PartialResBuff_Prev.Flag)
	{
		PartialResBuff_Curr.Inst = strdup(PartialResBuff_Prev.Inst);
		PartialResBuff_Curr.DReg = strdup(PartialResBuff_Prev.DReg);
		PartialResBuff_Curr.S1 = PartialResBuff_Prev.S1;
		PartialResBuff_Curr.S2 = PartialResBuff_Prev.S2;
		PartialResBuff_Curr.Flag = PartialResBuff_Prev.Flag;
		PartialResBuff_Prev.Flag = CLEAR;
	}

	for (i = 0; i < REB_SIZE; i++)
	{
		if(ResultBuff_Prev[i].Flag)
		{
			ResultBuff_Curr[i].DestReg = strdup(ResultBuff_Prev[i].DestReg);
			ResultBuff_Curr[i].DatValue = ResultBuff_Prev[i].DatValue;
			ResultBuff_Curr[i].Flag = ResultBuff_Prev[i].Flag;
			ResultBuff_Prev[i].Flag = CLEAR;
		}
	}

}

/* ******************* 			 END OF FILE 		****************** */
