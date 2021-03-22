//-----------------------------------------
// NAME: Al-amin Lawal
// STUDENT NUMBER: 7833358
// COURSE: Comp 3370
// INSTRUCTOR: Franklin Bristow
// ASSIGNMENT: Assignment 2
// PURPOSE: The purpose of this assignment is 
// to add a fully associative cache to the simulator
// implemented in assignment 1.
//-----------------------------------------

#include <stdio.h>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////////////////////////
// constants and structures

// constants for our processor definition (sizes are in words)
#define WORD_SIZE     2
#define DATA_SIZE     1024
#define CODE_SIZE     1024
#define REGISTERS     16

// number of bytes to print on a line
#define LINE_LENGTH   16

// our filled illegal instruction
#define MEM_FILLER    0xFF

// don't allow for more than 1000000 branches
#define BRANCH_LIMIT  1000000

// CACHE_BLOCKS is the number of blocks in our cache directory and cache memory arrays(basically number of blocks in our cache)
#define CACHE_BLOCKS 4

// Block size is the number of words per block
#define BLOCK_SIZE 2

// the length of a cache block offset
#define BLOCK_OFFSET		(int)log2(BLOCK_SIZE)

// since I restructured the data array to be a 3 Dimensional array, DATA_BLOCKS allows our data memory array
// to store DATA_SIZE(1024) words
// it is the number of blocks in our data memory array
#define DATA_BLOCKS (DATA_SIZE/BLOCK_SIZE)

// our opcodes are nicely incremental
enum OPCODES
{
  ADD_OPCODE,
  SUB_OPCODE,
  AND_OPCODE,
  OR_OPCODE,
  XOR_OPCODE,
  MOVE_OPCODE,
  SHIFT_OPCODE,
  BRANCH_OPCODE,
  NUM_OPCODES
};

typedef enum OPCODES Opcode;

// We have specific phases that we use to execute each instruction.
// We use this to run through a simple state machine that always advances to the
// next state and then cycles back to the beginning.
enum PHASES
{
  FETCH_INSTR,
  DECODE_INSTR,
  CALCULATE_EA,
  FETCH_OPERANDS,
  EXECUTE_INSTR,
  WRITE_BACK,
  NUM_PHASES,
  // the following are error return codes that the state machine may return
  ILLEGAL_OPCODE,    // indicates that we can't execute anymore instructions
  INFINITE_LOOP,     // indicates that we think we have an infinite loop
  ILLEGAL_ADDRESS,   // inidates that we have an memory location that's out of range
};

typedef enum PHASES Phase;

// We use a structure to maintain our current state. This allows for the information
// to be easily passed around.
struct STATE
{
  // internal registers used for system operation
  // note that these are all 16-bit meaning that all memory accesses will by 16-bit
  unsigned short PC;
  unsigned short MDR;
  unsigned short MAR;
  // making the IR a 2 byte array makes extracting the bits easier...
  // note that this could be done with a union or bit fields on a short
  unsigned char IR[2];
  
  // stores the actual operand data that we will manipulate
  // x and y are the ALU inputs and z is the output
  unsigned short ALU_x;
  unsigned short ALU_y;
  unsigned short ALU_z;
};

typedef struct STATE State;

// standard function pointer to run our control unit state machine
typedef Phase (*process_phase)(void);


// this structure represents a directory for every single block in the cache.
struct DIRECTORY
{
	// to account for compulsory misses
	bool valid;

	// since we are using write-back update policy
	bool dirty;

	// addresses are only 10 bits, so tag can always fit in a short
	unsigned short tag;

  // value that you use for tracking which entry in the directory is the least recently used.
	unsigned long reference_count;
};


////////////////////////////////////////////////////////////////////
// prototypes
Phase fetch_instr();
Phase decode_instr();
Phase calculate_ea();
Phase fetch_operands();
Phase execute_instr();
Phase write_back();
unsigned short load_data();
void store_data(unsigned short );
int read_block(unsigned short );
void write_block(int );
int lru_block();
void cache_flush();
void print_statistics();
int get_empty_block();
bool find_block( unsigned short, int &);


////////////////////////////////////////////////////////////////////
// local variables

// a count of branches so we can stop processing if we get an infinite loop
static int branch_count = 0;

// tracks cache hits and misses
static int hits;
static int misses;

// global counter variable to determine which cache block contains the least recently used entry
static unsigned long lru_global_counter;

// memory for our code, using our word size for a second dimension to make accessing bytes easier
static unsigned char code[CODE_SIZE][WORD_SIZE];

// restructured data array to have the same structure as our cache memory
// allows for easy reading and writing from/to this data memory array
static unsigned char data[DATA_BLOCKS][BLOCK_SIZE][WORD_SIZE]; 

// the cache directory
static struct DIRECTORY cache_directory[CACHE_BLOCKS];

// the cache memory array
static unsigned char cache_memory[CACHE_BLOCKS][BLOCK_SIZE][WORD_SIZE];

// our general purpose registers
// NOTE: we let the registers match the host endianness so that the operations are easier -- all mapping occurs at the MDR
static unsigned short registers[REGISTERS];

// tracks what we're currently doing
static State state;

// A list of handlers to process each state. Provides for a nice simple
// state machine loop and is easily extended without using a huge
// switch statement.
static process_phase control_unit[NUM_PHASES] =
{
  fetch_instr, decode_instr, calculate_ea, fetch_operands, execute_instr, write_back
};


//////////////////////////////////////////////////////////////////////////
// data extraction support routines

#define opcode() ((Opcode)(state.IR[0] >> 5))
#define mode()   ((state.IR[0] >> 2) & 0x07)

// pulls a literal value from the 2nd operand of the current instruction
char extract_literal()
{
  char value = (state.IR[1] & 0x3F);
  
  // sign extend if negative
  if ( value & 0x20 )
    value |= 0xC0;
  
  return value;
}


static unsigned char get_reg1()
{
  unsigned char reg1 = 0xFF;
  reg1 = ((state.IR[0]&0x03)<<2) | (state.IR[1]>>6);
  reg1 &= 0x0F;
  
  return reg1;  
}


//////////////////////////////////////////////////////////////////////////
// state processing routines -- note that they all have the same prototype

// pulls the instruction from code memory, verifying the address first
Phase fetch_instr()
{
  Phase rc = DECODE_INSTR;
  
  // make sure it's in range
  if ( state.PC < CODE_SIZE )
  {
    // using the MAR/MDR seems really weird here since you can just use the PC to index code[]
    // but, we should do it the way the CPU would handle things.
    state.MAR = state.PC;
    state.MDR = code[state.MAR][0];
    state.MDR <<= 8;
    state.MDR |= code[state.MAR][1];
    
    state.IR[0] = (unsigned char)(state.MDR >> 8);
    state.IR[1] = (unsigned char)(state.MDR & 0x00ff);
  }
  else
    rc = ILLEGAL_ADDRESS;
  
  return rc;
}


// pulls the opcode and addressing mode and verifies that the instruction is valid.
// uses the instruction characterization to decide where to go next.
Phase decode_instr()
{
  Phase rc = FETCH_OPERANDS;
  
  // validate the instruction before continuing
  switch( opcode() )
  {
      // valid modes are 000b and 001b
    case ADD_OPCODE:
    case SUB_OPCODE:
    case AND_OPCODE:
    case OR_OPCODE:
    case XOR_OPCODE:
    case SHIFT_OPCODE:
      if ( mode() > 1 )
        rc = ILLEGAL_OPCODE;
      break;
      
      // invalid mode if the second bit is set
    case MOVE_OPCODE:
      if ( mode() & 0x02 )
        rc = ILLEGAL_OPCODE;
      else
        rc = CALCULATE_EA;
      break;
      
      // invalid mode if all 3 bits are set
    case BRANCH_OPCODE:
      if ( mode() == 0x07 )
        rc = ILLEGAL_OPCODE;
      break;
      
      // all other opcode values are invalid  
    default:
      rc = ILLEGAL_OPCODE;
      break;
  }  
  return rc;
}

// effective address calculations are only required if we have to access memory
// with the address placed in the MAR
Phase calculate_ea()
{
  Phase rc = FETCH_OPERANDS;
  unsigned char reg = 0xFF;
  
  // the first operand has our memory address
  if ( mode() & 0x04 )
  {
    reg = get_reg1();
  }
  
  // the second operand has our memory address
  else if ( mode() & 0x01 )
  {
    reg = state.IR[1] >> 2;
    reg &= 0x0F;
  }
  
  // load the address if we have a valid register
  if ( reg != 0xFF )
  {
    state.MAR = registers[reg];
  }
  
  return rc;
}


// uses the instruction and addressing mode to decide how to get the data
Phase fetch_operands()
{
  Phase rc = EXECUTE_INSTR;
  unsigned char reg;
  
  // operand 1 is always register contents...
  // unless it's a MOVE, then it's a destination and doesn't need fetching
  if ( opcode() != MOVE_OPCODE )
  {
    reg = get_reg1();
    state.ALU_x = registers[reg];
  }
  
  // operand 2 is more complicated...
  
  // calculate a register value in case we need it
  reg = state.IR[1] >> 2;
  reg &= 0x0F;
  switch( opcode() )
  {
      // depending on the mode, put register contents or the literal into the "register"
    case ADD_OPCODE:
    case SUB_OPCODE:
    case AND_OPCODE:
    case OR_OPCODE:
    case XOR_OPCODE:
      if ( mode() == 0 )
        state.ALU_y = extract_literal();
      else
        state.ALU_y = registers[reg];
      break;
      
      // to simplify things we always put the data into the MDR, even for a literal to
      // register transfer. 
    case MOVE_OPCODE:
      // no need to execute an instruction here
      // though you could go through with the execute
      // phase and have it do *nothing*.
      rc = WRITE_BACK;
      
      // copy in the literal or register contents
      if ( (mode() & 0x01) == 0 )
        state.MDR = extract_literal();
      else if ( mode() & 0x04 )
      {
        state.MDR = registers[reg];
      }
      
      // otherwise, fetch from memory
      else
      {
        // this is a check to make sure that the tag extracted from the MAR does not exceed the number of blocks in main memory
          // otherwise it is an illegal address
        if ( (state.MAR >> BLOCK_OFFSET) < DATA_BLOCKS )
        {
          state.MDR = load_data();
        }
        else
        {
          rc = ILLEGAL_ADDRESS;
        }
      }
      break;
      
      // branches always have a literal, ignored for jumps...  
    case BRANCH_OPCODE:
      state.ALU_y = extract_literal();
      break;
      
    default:
      break;
  }
  
  return rc;
}


// based on the opcode, performs the operation on the ALU inputs
Phase execute_instr()
{
  Phase rc = WRITE_BACK;
  
  switch( opcode() )
  {
    case ADD_OPCODE:
      state.ALU_z = (short)state.ALU_x + (short)state.ALU_y;
      break;
      
    case SUB_OPCODE:
      state.ALU_z = (short)state.ALU_x - (short)state.ALU_y;
      break;
      
    case AND_OPCODE:
      state.ALU_z = state.ALU_x & state.ALU_y;
      break;
      
    case OR_OPCODE:
      state.ALU_z = state.ALU_x | state.ALU_y;
      break;
      
    case XOR_OPCODE:
      state.ALU_z = state.ALU_x ^ state.ALU_y;
      break;
      
    case SHIFT_OPCODE:
      if ( mode() == 0 )
        state.ALU_z = state.ALU_x >> 1;
      else
        state.ALU_z = state.ALU_x << 1;
      break;
      
    case BRANCH_OPCODE:
      // handle the jump separately since it's special
      if ( mode() == 0 )
      {
        state.ALU_z = state.ALU_x;
        
        // check for infinite loops
        branch_count++;
        if (branch_count > BRANCH_LIMIT )
          rc = INFINITE_LOOP;
      }
      
      else
      {
        bool branch = false;
        
        switch( mode() )
        {
            // BEQ
          case 1:
            if ( (short)state.ALU_x == (short)registers[0] )
              branch = true;
            break;
            
            // BNE
          case 2:
            if ( (short)state.ALU_x != (short)registers[0] )
              branch = true;
            break;
            
            // BLT
          case 3:
            if ( (short)state.ALU_x < (short)registers[0] )
              branch = true;
            break;
            
            // BGT
          case 4:
            if ( (short)state.ALU_x > (short)registers[0] )
              branch = true;
            break;
            
            // BLE
          case 5:
            if ( (short)state.ALU_x <= (short)registers[0] )
              branch = true;
            break;
            
            // BGE
          case 6:
            if ( (short)state.ALU_x >= (short)registers[0] )
              branch = true;
            break;
        }
        
        // we always update the PC, but it only changes if required
        if ( branch )
        {
          state.ALU_z = state.PC + state.ALU_y - 1;
          
          // check for infinite loops
          branch_count++;
          if (branch_count > BRANCH_LIMIT )
            rc = INFINITE_LOOP;
        }
        
        else
          // still need the PC in ALU_z for write back...
          state.ALU_z = state.PC;
      }
      break;
      
    default:
      break;
  }
  
  return rc;
}


// we will either write to a register, the PC or memory
Phase write_back()
{
  Phase rc = FETCH_INSTR;
  // determine the register we may have to write into
  unsigned char reg = get_reg1();
  
  switch( opcode() )
  {
      // always writing to a register
    case ADD_OPCODE:
    case SUB_OPCODE:
    case AND_OPCODE:
    case OR_OPCODE:
    case XOR_OPCODE:
    case SHIFT_OPCODE:
      registers[reg] = state.ALU_z;
      break;
      
      // update the PC, if no branch it will simply re-write itself  
    case BRANCH_OPCODE:
      state.PC = state.ALU_z;
      break;
      
    case MOVE_OPCODE:
      // do we put the contents of MDR into a register or memory?
      if ( mode() & 0x04 )
      {
        // memory

        // this is a check to make sure that the tag extracted from the MAR does not exceed the number of blocks in main memory
          // otherwise, it is an illegal address
        if ( (state.MAR >> BLOCK_OFFSET) < DATA_BLOCKS )
        {
          store_data(state.MDR);
        } else 
        {
          rc = ILLEGAL_ADDRESS;
        }
      }
      
      else
        // register
        registers[reg] = state.MDR;
      
      break;
      
    default:
      break;
  }
  
  // don't forget to increment the program counter
  state.PC++;
  
  return rc;
}


// This function copies a specified block in the cache to the appropriate location in main memory
void write_block(int ca_index)
{
  unsigned short address; // an address in main memory
  int i; // loop counter variable

  address = cache_directory[ca_index].tag;  // Specifies the block we should be writing to in main memory

  // make sure the cache block is valid
  // that is make sure this cache block contains a real cache entry that has been explicitly loaded from main memory
  if ( cache_directory[ca_index].valid ) {
    // writes a specified block in the cache to the appropriate location in main memory(the 3D data array)
    for( i=0 ; i<BLOCK_SIZE ; i++ )
    {
      data[address][i][0] = cache_memory[ca_index][i][0];
      data[address][i][1] = cache_memory[ca_index][i][1];
    }
  }
}


// this function returns the index of the first empty block in the cache
// tf there are no empty blocks, this function returns -1
int get_empty_block() {
  bool found = false; // for loop stop condition
  int empty_block_index = -1; // initially set to -1 to indicate that there are no empty blocks

  // loop through the cache directory,
  // if an empty cache block is found (valid bit is not set i.e valid = false),
  // exit the loop immediately and return the index of that empty cache block
  // if an empty cache block is not found, return -1
  for ( int i = 0; i < CACHE_BLOCKS && !found; i++ ) {
      if ( !(cache_directory[i].valid) ) {
          empty_block_index = i;
          found = true;
      }
  }
    return empty_block_index;
}


// finds the cache block containing the least recently used
// entry in the cache directory and returns the cache block index
// similar to the implementation of a getMin() function
int lru_block() {
  int lru_block = cache_directory[0].reference_count;  // assume the first block in the cache directory contains the least recently used entry
  int lru_block_index = 0;  // index of the cache block containing the least recently used entry

  // start looping through all cache blocks from the second cache block, assuming CACHE_BLOCKS > 1
  for ( int i = 1; i < CACHE_BLOCKS; i++ ) {
    // if we find a cache block with the least recently used entry, assign the index of that cache block to our lru_block_index variable
    if ( cache_directory[i].reference_count < lru_block ) { 
      lru_block = cache_directory[i].reference_count;
      lru_block_index = i;
    }
  }
  return lru_block_index;
}


// loads a block from main memory into the cache
// first checks to load an empty block in the cache
// if it does not find an empty block, it finds the least recently used block and loads data into it
// returns the index of cache block that the block from main memory was loaded into 
int read_block(unsigned short address)
{
  int i;   // loop counter variable
  int cache_index; // the cache index of the empty cache block or the cache block containing the least recently used entry
  unsigned short memory_address; // address in main memory

  // extracts the tag from the passed address
  memory_address = address;
  memory_address >>= BLOCK_OFFSET;

  // first checks to see if there is an empty cache block
  // recall from find_empty_block(), that it returns -1 if there are no empty cache blocks
  // if there is an empty cache block, assign the index of the empty cache block to cache_index
  // if there is not an empty cache block, then it gets the index of the cache block containing the least recently used entry
  if ( get_empty_block() > -1 ) {
    cache_index = get_empty_block();
  } else {
    cache_index = lru_block();  
  }
    
  // using write-back update policy
  // if the cache block is dirty, write that block to main memory
  // then set the dirty bit to 0(that is dirty = false)
  if( cache_directory[cache_index].dirty )
  {
    write_block(cache_index);
    cache_directory[cache_index].dirty = false;
  }

  // takes the cache index of the empty cache block 
  // or the cache index of the cache block containing the least recently used entry
  // make that cache block, the most recently used
  lru_global_counter = lru_global_counter + 1;
  cache_directory[cache_index].reference_count = lru_global_counter;

  // copies a block from main memory and stores it in the appropriate cache block
  for( i=0 ; i<BLOCK_SIZE; i++ )
  {
    cache_memory[cache_index][i][0] = data[memory_address][i][0];
    cache_memory[cache_index][i][1] = data[memory_address][i][1];
  }

  // The cache block is now valid since we have explicitly loaded data from main memory array into it 
  cache_directory[cache_index].valid = true; 
  cache_directory[cache_index].tag = memory_address;
  return cache_index;
}


// looks for the cache block in the cache directory with the same tag as the passed tag 
// returns true if found, false otherwise
// if found, sets block_index variable, so we can identify the cache block
bool find_block( unsigned short tag, int &block_index )
{
  bool found = false; // for loop stop condition
  int i;

  for ( i=0 ; i<CACHE_BLOCKS && !found ; i++ )
  {
    // make sure the cache block is valid and has the tag we are searching for
    if ( cache_directory[i].valid && (cache_directory[i].tag == tag))
    {
      block_index = i;
      found = true;
    }
  }
  return found;
}

// this function applies the demand fetch policy to check the cache for the requested data to load into the cache
// if the data is not in the cache, it will load it into the appropriate cache block from main memory
unsigned short load_data()
{   
  unsigned short data; // data eventually to be loaded to the MDR
  unsigned short memory_tag; // tag variable containing tag extracted from MAR
  int offset;  // offset variable containing offset extracted from MAR
  bool found; // boolean variable determining whether we have found the block containing the requested data to load 
  int block_index;  // index of a cache block in the cache

  memory_tag = state.MAR >> BLOCK_OFFSET; // extract tag from MAR
  offset = state.MAR & (BLOCK_SIZE - 1);  // extract offset from MAR

  // returns true if we found the cache block containing requested data to be eventually loaded into the MDR
  found = find_block(memory_tag, block_index );

  // if requested data to load to the MDR is in the cache
  // get the data from the appropriate cache block
  if (found) 
  {
    // Combine the two individual bytes to a word so we can load it into the MDR assuming big endian
    data = cache_memory[block_index][offset][0] << 8;
    data <<= 8;
    data |= cache_memory[block_index][offset][1];

    // set that cache block to block containing most recently used entry
    lru_global_counter = lru_global_counter + 1;
    cache_directory[block_index].reference_count = lru_global_counter;

    // track hits
    hits = hits + 1;

  }
  // if requested data to load to the MDR is not in the cache, load from main memory
  else
  {
    block_index = read_block( state.MAR );  // index of cache block that has just been loaded with block from main memory

    // Combine the two individual bytes to a word so we can load it into the MDR assuming big endian
    data = cache_memory[block_index][offset][0];
    data <<= 8;
    data |= cache_memory[block_index][offset][1];

    // track misses
    misses = misses + 1;
  }
  return data;
}


// stores data into the cache, if present
// if the data is not in the cache, load the data from main memory to the cache
void store_data(unsigned short memory_data)
{
  unsigned char data_byte1; // first byte of word(2 bytes) extracted from passed data (assuming BIG ENDIAN)
  unsigned char data_byte2; // second byte of word(2 bytes) extracted from passed data (assuming BIG ENDIAN)
  unsigned short memory_tag; // tag extracted from MAR
  int offset;  // offset extracted from MAR
  bool found; // for loop stop condition
  int block_index; // cache index

  memory_tag = state.MAR >> BLOCK_OFFSET;   // extract tag from MAR
  offset = state.MAR & (BLOCK_SIZE - 1);  // extract offset from MAR

  data_byte1 = memory_data >> 8;  // extracting the first byte of the word(2 bytes) from passed data (assuming BIG ENDIAN)
  data_byte2 = memory_data & 0x00FF;  // extracting the second byte of the word(2 bytes) from passed data (assuming BIG ENDIAN)

  // returns true if we found the cache block containing data to be stored into the cache is present
  found = find_block(memory_tag, block_index );

  // if the data is present in the cache, store the passed data(memory_data) into the appropriate cache block
  if (found) 
  {
    cache_memory[block_index][offset][0] = data_byte1;  // store the first byte extracted from passed data to appropriate cache block location
    cache_memory[block_index][offset][1] = data_byte2;  // store the second byte extracted from passed data to appropriate cache block location
    cache_directory[block_index].dirty = true;   // set the dirty bit of that cache index to 1(true)

    // set that cache index to be the most recently used by incrementing the global counter variable
    // and assigning it to the reference count of that cache block
    lru_global_counter = lru_global_counter + 1;
    cache_directory[block_index].reference_count = lru_global_counter;

    // track hits
    hits = hits + 1;
  }
  // if not in cache, load from memory
  else
  {    
    block_index = read_block( state.MAR ); // index of cache block that has just been loaded with block from main memory

    // use the cache index and offset to determine the appropriate location in the cache to store the 2 bytes extracted from passed data(memory_data)
    cache_memory[block_index][offset][0] = data_byte1;  // store the first byte extracted from passed data to appropriate cache block location
    cache_memory[block_index][offset][1] = data_byte2;  // store the second byte extracted from passed data to appropriate cache block location
    cache_directory[block_index].dirty = true;   // set the dirty bit of that cache index to 1(true)

    // track misses
    misses = misses + 1;  
  }
}


// this function flushes out dirty blocks in the cache to main memory after the program is complete
void cache_flush()
{
  int i; // loop counter variable

  // check each cache block for dirtiness
  // if cache block is dirty, write cache block to main memory
  for( i=0 ; i<CACHE_BLOCKS ; i++ )
  {
    if( cache_directory[i].dirty )
    {
      // write dirty cache block to memory
      write_block(i);
      cache_directory[i].dirty = false;
    }
  }
}


// Prints report indicating the cache hits, misses and hit rate achieved
void print_statistics()
{
  // if program does loads or stores, report the hits, misses and overall hit rate
  // if program does no loads or stores, report the hits as 0, misses as 0 and overall hit rate as 0.00 (This is done to prevent div by 0 error)
  if( hits + misses > 0 )
  {
    printf( "Cache report for fully associative cache with %d block(s) of %d word(s) each:\n", CACHE_BLOCKS, BLOCK_SIZE);
    printf( "Hits: %d\nMisses: %d\n", hits, misses);
    printf( "Overall hit rate: %.2f%%\n\n", ((float)hits / (float)(hits + misses))*100);
  } 
  else 
  {
    printf( "Cache report for fully associative cache with %d block(s) of %d word(s) each:\n", CACHE_BLOCKS, BLOCK_SIZE);
    printf( "Hits: %d\nMisses: %d\n", hits, misses);
    printf( "Overall hit rate: %.2f%%\n\n", 0.0);
  }
}


////////////////////////////////////////////////////////////////////
// general routines


// Initialize memory and registers with default values (0xFF and 0 respectively)
void initialize_system()
{
  int i;
  int j;
  
  state.PC = 0;
  state.MDR = 0;
  state.MAR = 0;
  state.ALU_x = 0;
  state.ALU_y = 0;
  state.ALU_z = 0;

  // intializes hit and miss tracking
 	hits = 0;
  misses = 0;

  // initialize the least recently used global counter
  lru_global_counter = 0;
  
  // fill all of our code and data space
  for ( i=0 ; i<CODE_SIZE ; i++ )
  {
    code[i][0] = MEM_FILLER;
    code[i][1] = MEM_FILLER;
  }

  for ( i=0 ; i<DATA_BLOCKS ; i++ ) {
  	for (j = 0; j < BLOCK_SIZE; j++)
		  {
		    data[i][j][0] = MEM_FILLER;
		    data[i][j][1] = MEM_FILLER;
		  }
  }
  
  // initialize our registers
  for ( i=0 ; i<REGISTERS ; i++ )
    registers[i] = 0;

	// initialize the valid bit, dirty bit and value used to track the reference count
  // for every cache block in the cache directory
  for ( i=0 ; i<CACHE_BLOCKS ; i++ )
  {
	 	cache_directory[i].valid = false;
	  cache_directory[i].dirty = false;
    cache_directory[i].reference_count = 0;
  }

  // fill our cache memory array with invalid data
  for (i = 0; i < CACHE_BLOCKS; i ++){
  	for (j = 0; j < BLOCK_SIZE; j++) {
  		cache_memory[i][j][0] = MEM_FILLER;
  		cache_memory[i][j][1] = MEM_FILLER;
  	}
  }
}


// checks the hex value to ensure it a printable ASCII character. If
// it isn't, '.' is returned instead of itself
char valid_ascii( unsigned char hex_value )
{
  if ( hex_value < 0x21 || hex_value > 0x7e )
    hex_value = '.';
  
  return (char)hex_value;
}


// takes the data and prints it out in hexadecimal and ASCII form
void print_memory()
{
  int count = 0;
  int ix = 0;
  int jx = 0;
  int text_index = 0;
  char the_text[LINE_LENGTH+1];

  // print each line 1 at a time
  for ( ix=0 ; ix<DATA_BLOCKS ; ix++ ) {
  	for (jx = 0; jx < BLOCK_SIZE; jx++) {
  		if ( text_index == 0 )
		  {
		    // for each word we're printing 2 bytes, so the counter
		    // should be twice count.
		    printf( "%08x  ", count*2 );
		  }

		  the_text[text_index++] = valid_ascii((unsigned char)(data[ix][jx][0]));
	    the_text[text_index++] = valid_ascii((unsigned char)(data[ix][jx][1]));
	    printf( "%02x %02x ", data[ix][jx][0], data[ix][jx][1] );

      count++;
    }

    // print out a line if we're at the end
    if ( text_index == LINE_LENGTH )
    {
      text_index = 0;
      the_text[LINE_LENGTH] = '\0';
      printf( " |%s|\n", the_text );
    }
  }
}


// converts the passed string into binary form and inserts it into our data area
// assumes an even number of words!!!
void insert_data( string line )
{
  unsigned int  i;
  char          ascii_data[5];
  unsigned char byte1;
  unsigned char byte2;
  
  ascii_data[4] = '\0';
  
  static int blocks = 0;   // block counter in main memory
  static int words = 0;    // word counter in a block in main memory


  for (i=0 ; i<line.length() ; i+=4 )
  {
  	ascii_data[0] = line[i];
    ascii_data[1] = line[i+1];
    ascii_data[2] = line[i+2];
    ascii_data[3] = line[i+3];
      
    sscanf(ascii_data, "%02hhx%02hhx", &byte1, &byte2);

    // Fills up a block in main memory before moving to next block
    if ( blocks < DATA_BLOCKS ) {
      if ( words < BLOCK_SIZE ) {
        data[blocks][words][0] = byte1;
        data[blocks][words][1] = byte2;
      } 
      words++;

      // If we have filled up a block in main memory with enough words, move to the next block 
      if ( words >= BLOCK_SIZE ) {
      	blocks++;
      	words = 0;
    	}
    }    
  }
}


// reads in the file data and returns true is our code and data areas are ready for processing
bool load_files( const char *code_filename, const char *data_filename )
{
  FILE           *code_file = NULL;
  std::ifstream  data_file( data_filename );
  string         line;           // used to read in a line of text
  bool           rc = false;
  
  // using RAW C here since I want to have straight binary access to the data
  code_file = fopen( code_filename, "r" );
  
  // since we're allowing anything to be specified, make sure it's a file...
  if ( code_file )
  {
    // put the code into the code area
    fread( code, 1, CODE_SIZE*WORD_SIZE, code_file );
    
    fclose( code_file );
    
    // since we're allowing anything to be specified, make sure it's a file...
    if ( data_file.is_open() )
    {
      // read the data into our data area
      getline( data_file, line );
      while ( !data_file.eof() )
      {
        // put the data into the data area
        insert_data( line );
        
        getline( data_file, line );
      }
      data_file.close();
      
      // both files were read so we can continue processing
      rc = true;
    }
  }
  
  return rc;
}


// runs our simulation after initializing our memory
int main (int argc, const char * argv[])
{
  Phase current_phase = FETCH_INSTR;  // we always start if an instruction fetch
  
  initialize_system();
  
  // read in our code and data
  if ( load_files( argv[1], argv[2] ) )
  {
    // run our simulator
    while ( current_phase < NUM_PHASES ) {
      current_phase = control_unit[current_phase]();
    }

    cache_flush();
    print_statistics();
    
    // output what stopped the simulator
    switch( current_phase )
    {
      case ILLEGAL_OPCODE:
        printf( "Illegal instruction %02x%02x detected at address %04x\n\n",
               state.IR[0], state.IR[1], state.PC );
        break;
        
      case INFINITE_LOOP:
        printf( "Possible infinite loop detected with instruction %02x%02x at address %04x\n\n",
               state.IR[0], state.IR[1], state.PC );
        break;
        
      case ILLEGAL_ADDRESS:
        printf( "Illegal address %04x detected with instruction %02x%02x at address %04x\n\n",
               state.MAR, state.IR[0], state.IR[1], state.PC );
        break;
        
      default:
        break;
    }
    
    // print out the data area
    print_memory();
  }
}
