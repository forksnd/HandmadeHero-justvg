struct sort_entry
{
    r32 SortKey;
    u32 Index;
};

inline void
Swap(sort_entry *A, sort_entry *B)
{
    sort_entry Store = *B;
    *B = *A;
    *A = Store;
}

internal void
BubbleSort(u32 Count, sort_entry *First)
{
    for(u32 Outer = 0;
        Outer < Count;
        Outer++)
    {
        b32 ListIsSorted = true;
        for(u32 Inner = 0;
            Inner < (Count - 1);
            Inner++)
        {
            sort_entry *EntryA = First + Inner;
            sort_entry *EntryB = EntryA + 1;

            if(EntryA->SortKey > EntryB->SortKey)
            {
                Swap(EntryA, EntryB);
                ListIsSorted = false;
            }
        }

        if(ListIsSorted)
        {
            break;
        }
    }
}

inline u32 
SortKeyToU32(r32 SortKey)
{
    // NOTE(georgy): We need to turn our 32-bit floating point value
    // into some strictly ascending 32-bit unsigned integer value
    u32 Result = *(u32 *)&SortKey;
    if(Result & 0x80000000)
    {
        Result = ~Result;
    }
    else
    {
        Result |= 0x80000000;
    }

    return(Result);
}

internal void
RadixSort(u32 Count, sort_entry *Entries, sort_entry *Temp)
{
    sort_entry *Source = Entries;
    sort_entry *Dest = Temp;

    for(u32 ByteIndex = 0;
        ByteIndex < 32;
        ByteIndex += 8)
    {
        u32 SortKeyOffset[256] = {};

        // NOTE(georgy): First pass - count how many of each key
        for(u32 Index = 0;
            Index < Count;
            Index++)
        {
            u32 RadixValue = SortKeyToU32(Source[Index].SortKey);
            u32 RadixPiece = (RadixValue >> ByteIndex) & 0xFF;
            SortKeyOffset[RadixPiece]++;
        }

        // NOTE(georgy): Change counts to offsets
        u32 Total = 0;
        for(u32 SortKeyIndex = 0;
            SortKeyIndex < ArrayCount(SortKeyOffset);
            SortKeyIndex++)
        {
            u32 Count = SortKeyOffset[SortKeyIndex];
            SortKeyOffset[SortKeyIndex] = Total;
            Total += Count;
        }

        // NOTE(georgy): Second pass - place elements into the right location
        for(u32 Index = 0;
            Index < Count;
            Index++)
        {
            u32 RadixValue = SortKeyToU32(Source[Index].SortKey);
            u32 RadixPiece = (RadixValue >> ByteIndex) & 0xFF;
            Dest[SortKeyOffset[RadixPiece]++] = Source[Index];
        }

        sort_entry *SwapTemp = Dest;
        Dest = Source;
        Source = SwapTemp; 
    }
}

internal void
MergeSort(u32 Count, sort_entry *First, sort_entry *Temp)
{
    if(Count == 1)
    {
        // NOTE(georgy): No work to do.
    }
    else if(Count == 2)
    {
        sort_entry *EntryA = First;
        sort_entry *EntryB = EntryA + 1;

        if(EntryA->SortKey > EntryB->SortKey)
        {
            Swap(EntryA, EntryB);
        }
    }
    else
    {
        u32 Half0 = Count / 2;
        u32 Half1 = Count - Half0;

        Assert(Half0 >= 1);
        Assert(Half1 >= 1);
        
        sort_entry *InHalf0 = First;
        sort_entry *InHalf1 = First + Half0;
        sort_entry *End = First + Count;

        MergeSort(Half0, InHalf0, Temp);
        MergeSort(Half1, InHalf1, Temp);

        sort_entry *ReadHalf0 = InHalf0;
        sort_entry *ReadHalf1 = InHalf1;
        
        sort_entry *Out = Temp;
        for(u32 Index = 0;
            Index < Count;
            Index++)
        {
            if(ReadHalf0 == InHalf1)
            {
                *Out++ = *ReadHalf1++;
            }
            else if(ReadHalf1 == End)
            {
                *Out++ = *ReadHalf0++;
            }
            else if(ReadHalf0->SortKey < ReadHalf1->SortKey)
            {
                *Out++ = *ReadHalf0++;
            }
            else
            {
                *Out++ = *ReadHalf1++;;
            }
        }
        Assert(Out == (Temp + Count));
        Assert((ReadHalf0 == InHalf1) && (ReadHalf1 == End));

        // TODO(georgy): Not really necessary if we ping-pong
        for(u32 Index = 0;
            Index < Count;
            Index++)
        {
            First[Index] = Temp[Index];
        }
    }
}
