#include "VirtualMemory.h"
#include "PhysicalMemory.h"

/// Methods ////

 /**
  * Returns the Offset from the VirtualAddress
  */
uint64_t GetOffset(uint64_t virtualAddress)
{
    uint64_t temp (1 << OFFSET_WIDTH);
    return virtualAddress & (temp - 1);
}


/**
 * Checks if the currentTable is empty by checking if all the frame cells with zero Value
 */
bool CheckFrameEmpty(word_t currentTable)
{
    word_t childrenValue;
    for (int i = 0; i < PAGE_SIZE ; i++)
    {
        PMread(currentTable * PAGE_SIZE + i,&childrenValue);
        if (childrenValue != 0)
            return false;
    }
    return true;
}

/**
     * Checks if the cyclic distance between page and address is greater then current maximal distance,
     * if so we will updated the parameters of current maximal distance frame.
 */
void CheckCyclicDistance(uint64_t addressWithoutOffset,word_t currentFrame,uint64_t currentFrameAddress,uint64_t* currentMaxDistance,
                         word_t* maxDistanceFrame,uint64_t* maxDistanceAddress,uint64_t* maxDistanceParentAddress,
                         uint64_t currentFrameParentAddress)
{
    uint64_t cyclicDistance = (currentFrameAddress > addressWithoutOffset) ?
                              currentFrameAddress - addressWithoutOffset :
                              addressWithoutOffset - currentFrameAddress;
    uint64_t compare = NUM_PAGES - cyclicDistance;
    uint64_t minDistance = (cyclicDistance > compare) ? compare : cyclicDistance;
    if (minDistance > *currentMaxDistance)
    {
        *currentMaxDistance = minDistance;
        *maxDistanceFrame = currentFrame;
        *maxDistanceAddress = currentFrameAddress;
        *maxDistanceParentAddress = currentFrameParentAddress;
    }
}

/**
 * This function runs on the frame table searching for an available frame,
 * if found a frame which all children's are 0 - return it
 */
word_t TraversTree(uint64_t addressWithoutOffset,
                   word_t currentFrame,
                   uint64_t currentFrameParentAddress,
                   uint64_t currentFrameAddress,
                   word_t parentOfLookingFrame,
                   int *maximalFrameIndex,
                   int currentDepth,
                   uint64_t* currentMaxDistance,
                   word_t* maxDistanceFrame,
                   uint64_t* maxDistanceAddress,
                   uint64_t* maxDistanceParentAddress)
{
    /// 1. if recursive call is on leaf - check the cyclic distance from the leaf to address
    if (currentDepth == TABLES_DEPTH)
    {
        CheckCyclicDistance(addressWithoutOffset,currentFrame,currentFrameAddress,currentMaxDistance,maxDistanceFrame,
                            maxDistanceAddress,maxDistanceParentAddress,currentFrameParentAddress);
        return 0;
    }

    /// 2. if found a frame that all of it children's are 0, reset the values of children's and return it
    if (CheckFrameEmpty(currentFrame)  && currentFrame != parentOfLookingFrame)
    {
        PMwrite(currentFrameParentAddress ,0);
        return currentFrame;
    }

    /// 3. run TraversTree on every children found that his value is not 0
    word_t childrenValue;
    for (int i = 0; i < PAGE_SIZE ; i++)
    {
        PMread(currentFrame * PAGE_SIZE + i,&childrenValue);
        if (childrenValue != 0)
        {
            /// 3.1 if found a frame with bigger value then current maximal frame index, update it
            if (childrenValue > *maximalFrameIndex)
                *maximalFrameIndex = childrenValue;

            /// 3.2 run the recursive call
            word_t frameFoundBySearch = TraversTree(addressWithoutOffset,
                                                    childrenValue,
                                                 currentFrame * PAGE_SIZE + i,
                                                 (currentFrameAddress << OFFSET_WIDTH) + i,
                                                    parentOfLookingFrame,
                                                    maximalFrameIndex,
                                                 currentDepth + 1,
                                                    currentMaxDistance,
                                                    maxDistanceFrame,
                                                    maxDistanceAddress,
                                                    maxDistanceParentAddress);

            /// 3.3 if found an available frame, return it
            if(frameFoundBySearch != 0)
                return frameFoundBySearch;
        }
    }
    return 0;
}

/**
 * This functions search for an available frame by using the TraversTree function.
 * it will return a frame by this priority:
 * empty frame -> maximal frame smaller than available number of frames -> most distinct frame
 * @param parentOfLookingFrame - we will use this parameter to make sure that we wont replace the parent frame
 */
word_t FindAvailableFrame(uint64_t addressWithoutOffset, word_t parentOfLookingFrame)
{
    /// 0. initialise all variables that will be given as reference to TraversTree
    uint64_t maxDistance = 0;
    word_t maxDistanceFrame = 0;
    uint64_t maxDistanceAddress = 0;
    uint64_t maxDistanceParentAddress = 0;
    int maximalFrameIndex = 0;

    /// 1 run the search algorithm on tree
    word_t FrameFound = TraversTree(addressWithoutOffset,
                                    0,
                                    0,
                                    0,
                                    parentOfLookingFrame,
                                    &maximalFrameIndex,
                                    0,
                                    &maxDistance,
                                    &maxDistanceFrame,
                                    &maxDistanceAddress,
                                    &maxDistanceParentAddress);

    /// 2. FrameFound will be different then 0 if TraversTree found a frame that all of it children are 0
    if (FrameFound != 0)
        return FrameFound;

    /// 3. Otherwise check if the maximal frame index found is smaller then possible NUM_FRAMES
    else if (maximalFrameIndex + 1 < NUM_FRAMES)
        return maximalFrameIndex + 1;

    /// 4. If none of the above - evict the most distinct frame and return it
    else
    {
        PMevict(maxDistanceFrame,maxDistanceAddress);
        PMwrite(maxDistanceParentAddress,0);
        return maxDistanceFrame;
    }

}

/**
 * This function Splits the addressWithoutOffset bits into threeDepthAddress as instructions of the
 * path of the VirtualMemory address
 */
void SplitAddress(uint64_t addressWithoutOffset,uint64_t* treeDepthsAddress)
{
    uint64_t addressWithoutOffsetToSplit = addressWithoutOffset;

    // 2.2.2 for TABLES_DEPTH number of times - set the i depth address to the i cell of treeDepthsAddress array
    for (uint64_t i = 0 ; i < TABLES_DEPTH ; i++)
    {
        treeDepthsAddress[TABLES_DEPTH - i - 1] = addressWithoutOffsetToSplit & (PAGE_SIZE - 1);
        addressWithoutOffsetToSplit = addressWithoutOffsetToSplit >> OFFSET_WIDTH;
    }
}

/**
 * This function Reset Frame by changing all the values to zero
 */
void ResetFrame(word_t frame)
{
    for (uint64_t j = 0 ; j < PAGE_SIZE ; j++)
    {
        PMwrite(frame * PAGE_SIZE + j,0);
    }
}

/**
 * During translation of virtual address to physical one,
 * Whenever we encounter an empty frame -  we will create a new frame for it.
 * @param ind - the index of the empty frame from the treeDepthsAddress array
 */
word_t FaultPageHandler(uint64_t addressWithoutOffset,word_t parentFrameAddress,uint64_t treeDepthsAddress[],
                        word_t currentFrameAddress, int ind)
{
    word_t frameFound = FindAvailableFrame(addressWithoutOffset, parentFrameAddress);

    if (ind == TABLES_DEPTH-1)
        PMrestore(frameFound,addressWithoutOffset);
    else
        ResetFrame(frameFound);

    PMwrite(parentFrameAddress * PAGE_SIZE + treeDepthsAddress[ind], frameFound);
    currentFrameAddress = frameFound;
    return currentFrameAddress;
}


/**
 * this function find a PhysicalMemory Address for a specific VirtualAddress
 */
uint64_t FindPhysicalAddress(uint64_t virtualAddress)
{
    /// 1. Get the offset
    uint64_t offset = GetOffset(virtualAddress);

    /// 2. split address to tree depths instructions
    uint64_t treeDepthsAddress[TABLES_DEPTH];
    uint64_t addressWithoutOffset = virtualAddress >> OFFSET_WIDTH; // bit manipulation to get address without offset
    SplitAddress(addressWithoutOffset,treeDepthsAddress);

    /// 3. Get physical address of the virtual one
    word_t currentFrameAddress = 0 ;
    for (int i =0 ; i < TABLES_DEPTH ; i++ )
    {
        word_t parentFrameAddress = currentFrameAddress;
        PMread(currentFrameAddress * PAGE_SIZE + treeDepthsAddress[i], &currentFrameAddress);
        if (currentFrameAddress == 0)
        {
            // Handle empty frame found
            currentFrameAddress = FaultPageHandler(addressWithoutOffset,parentFrameAddress,
                                                   treeDepthsAddress,currentFrameAddress,i);
        }
    }
    return currentFrameAddress * PAGE_SIZE + offset;
}


/// API ////

/**
 * Initialize the virtual memory.
 * will fill everet page of the frame with 0
 */
void VMinitialize()
{
    for (uint64_t i = 0 ; i < PAGE_SIZE ; i++)
    {
        PMwrite(i,0);
    }
}


int VMread(uint64_t virtualAddress, word_t* value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
        return 0;

    uint64_t physicalAddress = FindPhysicalAddress(virtualAddress);

    PMread(physicalAddress , value);
    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
        return 0;

    uint64_t physicalAddress = FindPhysicalAddress(virtualAddress);

    PMwrite(physicalAddress , value);
    return 1;
}

