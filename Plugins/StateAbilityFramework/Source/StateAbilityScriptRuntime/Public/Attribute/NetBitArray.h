#pragma once

#include "CoreMinimal.h"

class STATEABILITYSCRIPTRUNTIME_API FNetBitArray
{
public:
    typedef uint32 WordType;

    FNetBitArray();
    FNetBitArray(int32 Len);
    FNetBitArray(const FNetBitArray& Other);
    FNetBitArray(FNetBitArray&& Other);
    
    ~FNetBitArray();

    FORCEINLINE bool Add(int32 Index);
    // Range[BeginIndex, EndIndex] include EndIndex, not Range[BeginIndex, EndIndex)
    bool AddRange(int32 BeginIndex, int32 EndIndex);
    FORCEINLINE bool Remove(int32 Index);
    bool RemoveRange(int32 BeginIndex, int32 EndIndex);
    FORCEINLINE bool Clear();
    FORCEINLINE bool MarkAll();
    FORCEINLINE bool IsEmpty() const;
    FORCEINLINE bool IsDirty(int32 Index) const;
    FORCEINLINE int32 GetSize() const;
    FORCEINLINE int32 GetLen() const;

    FORCEINLINE FNetBitArray& operator = (const FNetBitArray& Other);
    FORCEINLINE FNetBitArray& operator = (FNetBitArray&& Other);
    FORCEINLINE FNetBitArray& operator &= (const FNetBitArray& Other);
    FORCEINLINE FNetBitArray& operator |= (const FNetBitArray& Other);
    
    friend FArchive& operator<<(FArchive& Ar, const FNetBitArray& A)
    {
        int32 RealBitSize = 0;
        if (Ar.IsSaving())
        {
            for (int32 Index = A.BitSize - 1; Index >= 0; --Index)
            {
                if (A.BitData[Index] != 0)
                {
                    RealBitSize = Index + 1;
                    break;
                }
            }
        }

        Ar << RealBitSize;
        for (int32 Index = 0; Index < RealBitSize; ++Index)
        {
            if (Index < A.BitSize)
            {
                Ar << A.BitData[Index];
            }
            else
            {
                // Drop it
                WordType Bit;
                Ar << Bit;
            }
        }
        return Ar;
    }

    class FIterator
    {
    public:
        FORCEINLINE FIterator(const FNetBitArray& Array)
            : BitIndex(0)
            , MaxIndex(-1)
            , CurrentWord(0)
            , BitArray(Array)
        {
            for (int32 Index = BitArray.BitSize - 1; Index >= 0; --Index)
            {
                if (BitArray.BitData[Index])
                {
                    MaxIndex = Index;
                    break;
                }
            }

            for (int32 Index = 0; Index <= MaxIndex; ++Index)
            {
                if (BitArray.BitData[Index])
                {
                    BitIndex = Index;
                    CurrentWord = BitArray.BitData[Index];
                    break;
                }
            }
        }

        FORCEINLINE operator bool()
        {
            if (BitIndex < MaxIndex)
            {
                return true;
            }
            return CurrentWord != 0;
        }
        
        FORCEINLINE FIterator& operator++()
        {
            CurrentWord &= CurrentWord - 1;
            for (; CurrentWord == 0 && BitIndex < MaxIndex;)
            {
                CurrentWord = BitArray.BitData[++BitIndex];
            }
            return *this;
        }

        // From http://graphics.stanford.edu/~seander/bithacks.html
        FORCEINLINE static int32 Log2(WordType Power2Num)
        {
            //x=2^k
            static const int32 Log2Map[32]={31,0,27,1,28,18,23,2,29,21,19,12,24,9,14,3,30,26,17,22,20,11,8,13,25,16,10,7,15,6,5,4};
            return Log2Map[Power2Num*263572066>>27];
        }

        FORCEINLINE int32 operator * () const
        {
            return BitIndex * WordSize + Log2(CurrentWord ^ (CurrentWord & (CurrentWord - 1)));
        }

    protected:
        int32 BitIndex;
        int32 MaxIndex;
        WordType CurrentWord;
        const FNetBitArray& BitArray;
    };
    
	class FFullIterator
	{
	public:
		FORCEINLINE FFullIterator(const FNetBitArray& Array)
			: BitIndex(0)
			, MaxIndex(-1)
			, WordIndex(0)
			, BitArray(Array)
		{
			MaxIndex = BitArray.BitSize - 1;
		}

		FORCEINLINE operator bool()
		{
			if (GetIndex() < BitArray.GetLen())
			{
				return true;
			}
			return false;
		}

		FORCEINLINE FFullIterator& operator++()
		{
			if (++WordIndex >= (WordSize - 1) && BitIndex < MaxIndex)
			{
				WordIndex = 0;
			}
			return *this;
		}

		FORCEINLINE int32 operator * () const
		{
			return GetIndex();
		}

		FORCEINLINE int32 GetIndex() const
		{
			return BitIndex * WordSize + WordIndex;
		}

		FORCEINLINE bool IsMarked() const
		{
			return BitArray.IsDirty(GetIndex());
		}

	protected:
		int32 BitIndex;
		int32 MaxIndex;
		int32 WordIndex;
		const FNetBitArray& BitArray;
	};

protected:
    static constexpr WordType FullWordMask = (WordType)-1;
    static constexpr int32 WordSize = sizeof(WordType) * 8;
    static constexpr int32 UnitWordMask = WordSize - 1;

    int32 BitLength;
    int32 BitSize;
    WordType* BitData;
};
