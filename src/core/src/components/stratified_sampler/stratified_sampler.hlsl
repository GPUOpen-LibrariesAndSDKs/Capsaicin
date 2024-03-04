/**********************************************************************
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef STRATIFIED_SAMPLER_HLSL
#define STRATIFIED_SAMPLER_HLSL

// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<uint> g_SeedBuffer;
StructuredBuffer<uint> g_SobolXorsBuffer;

#include "../../math/random.hlsl"

namespace NoExport
{
    /**
     * Generate a random number.
     * @param index Index into the sequence of value to return.
     * @param seed  The random number seed used for creating the sequence.
     * @return The new random number.
     */
    uint randomHash(uint index, uint seed)
    {
        uint state = (index + seed) * 747796405U + seed;
        state = state * 747796405u + 2891336453u;
        uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        word = (word >> 22u) ^ word;
        return word;
    }

    /**
     * Generate a number using stochastic sobol sequence.
     * @param index         Index into the sequence of value to return.
     * @param seed          The random number seed used for creating the sequence.
     * @param dimension     The current dimension of the sequence (0-indexed).
     * @param numDimensions The total number of dimensions that the sequence will be generated over.
     * @return The new random number (range  [0, uint::max)).
     */
    uint stochasticSobol(uint index, uint seed, uint dimension, uint numDimensions)
    {
        // Stochastic Generation of (t,s) Sample Sequences - Helmer
        uint bits = randomHash(index * numDimensions + dimension, seed);
        uint mostSigBit = firstbithigh(index);
        while (index > 0)
        {
            uint nextIndex = index ^ (1U << mostSigBit);
            nextIndex ^= g_SobolXorsBuffer[dimension * 32 + mostSigBit];
            uint nextMostSigBit = firstbithigh(nextIndex);
            uint randBits = randomHash(nextIndex * numDimensions + dimension, seed);
            uint bitSet = (mostSigBit - nextMostSigBit);
            randBits = (randBits >> (32U - bitSet)) ^ 1U;
            bits = (randBits << (32U - bitSet)) ^ (bits >> bitSet);

            mostSigBit = nextMostSigBit;
            index = nextIndex;
        }
        return bits;
    }
}

/**
 * Generate random numbers using a progressive stratified Sobol sampling function.
 * Each new sample is taken from the next available Sobol sequence dimension.
 * This should only be used when each call to one of this classes rand functions is sampling from a different distribution.
 */
class StratifiedSampler
{
    uint index;
    uint seed;
    uint dimension;

    /**
     * Generate a random uint.
     * @return The new random number (range  [0, 2^32)).
     */
    uint randInt()
    {
        dimension = (dimension <= 62) ? dimension + 1 : 0;
        return NoExport::stochasticSobol(index, seed, dimension, 0);
    }

    /**
     * Generate a random uint between 0 and a requested maximum value.
     * @return The new random number (range  [0, max)).
     */
    uint randInt(uint max)
    {
        // Unbiased Bitmask with Rejection
        uint mask = ~uint(0);
        --max;
        mask >>= 31 - firstbithigh(max | 1);
        uint ret;
        do
        {
            ret = randInt() & mask;
        } while (ret > max);
        return ret;
    }

    /**
     * Generate a random number.
     * @return The new number (range [0->1.0)).
     */
    float rand()
    {
        dimension = (dimension <= 62) ? dimension : 0;
        uint val = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        // Note: Use the upper 24 bits to avoid a bias due to floating point rounding error.
        float ret = (float)(val >> 8) * 0x1.0p-24f;
        return ret;
    }

    /**
     * Generate 2 random numbers.
     * @return The new numbers (range [0->1.0)).
     */
    float2 rand2()
    {
        dimension = (dimension <= 61) ? dimension : 0;
        // Get next Sobol values
        uint val0 = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        uint val1 = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        return float2(val0 >> 8, val1 >> 8) * 0x1.0p-24f.xx;
    }

    /**
     * Generate 3 random numbers.
     * @return The new numbers (range [0->1.0)).
     */
    float3 rand3()
    {
        dimension = (dimension <= 60) ? dimension : 0;
        // Get next Sobol values
        uint val0 = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        uint val1 = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        uint val2 = NoExport::stochasticSobol(index, seed, ++dimension, 64);
        return float3(val0 >> 8, val1 >> 8, val2 >> 8) * 0x1.0p-24f.xxx;
    }
};

/**
 * Initialise a stratified sample generator.
 * @param seed Seed value to initialise random with (e.g. 1D pixel index).
 * @param frame Temporal seed value to initialise random with (e.g. frame number).
 * @returns The new stratified sampler.
 */
StratifiedSampler MakeStratifiedSampler(uint seed, uint frame)
{
    StratifiedSampler ret =
    {
        frame,
        g_SeedBuffer[seed],
        0,
    };
    return ret;
}

/**
 * Generate 1D number sequence using a progressive stratified Sobol sampling function.
 * Each new sample is taken from the same Sobol sequence.
 */
class StratifiedSampler1D
{
    uint index;
    uint seed;
    uint dimension;

    /**
     * Generate a random uint.
     * @return The new random number (range  [0, 2^32)).
     */
    uint randInt()
    {
        return NoExport::stochasticSobol(index++, seed, dimension, 1);
    }

    /**
     * Generate a random uint between 0 and a requested maximum value.
     * @return The new random number (range  [0, max)).
     */
    uint randInt(uint max)
    {
        // Unbiased Bitmask with Rejection
        uint mask = ~uint(0);
        --max;
        mask >>= 31 - firstbithigh(max | 1);
        uint ret;
        do
        {
            ret = randInt() & mask;
        } while (ret > max);
        return ret;
    }

    /**
     * Generate a random number.
     * @return The new number (range [0->1.0)).
     */
    float rand()
    {
        uint val = randInt();
        float ret = (float)(val >> 8) * 0x1.0p-24f;
        return ret;
    }
};

/**
 * Initialise a 1D sequence stratified sample generator.
 * @param seed Seed value to initialise random with (e.g. 1D pixel index).
 * @returns The 1D new stratified sampler.
 */
StratifiedSampler1D MakeStratifiedSampler1D(uint seed)
{
    StratifiedSampler1D ret =
    {
        0,
        seed,
        0,
    };
    return ret;
}

/**
 * Initialise a 1D sequence stratified sample generator from an dimensional stratified sampler.
 * This can be used to branch off from an existing StratifiedSampler and locally generate additional samples
 *  from within the same dimension.
 * @param strat The dimensional sampler to initialise from.
 * @param offset (Optional) The number of values expected to be taken with this sampler that is used to offset the start index accordingly.
 * @returns The new 1D stratified sampler.
 */
StratifiedSampler1D MakeStratifiedSampler1D(StratifiedSampler strat, uint offset = 0)
{
    StratifiedSampler1D ret =
    {
        strat.index * offset,
        strat.seed,
        strat.dimension,
    };
    return ret;
}

/**
 * Generate 2D number sequence using a progressive stratified Sobol sampling function.
 * Each new sample is taken from the same Sobol sequence.
 */
class StratifiedSampler2D
{
    uint index;
    uint seed;
    uint dimension;

    /**
     * Generate 2 random numbers.
     * @return The new numbers (range [0->1.0)).
     */
    float2 rand2()
    {
        // Get next Sobol values
        uint val0 = NoExport::stochasticSobol(index, seed, dimension, 2);
        uint val1 = NoExport::stochasticSobol(index++, seed, dimension + 1, 2);
        return float2(val0 >> 8, val1 >> 8) * 0x1.0p-24f.xx;
    }
};

/**
 * Initialise a 2D sequence stratified sample generator.
 * @param seed Seed value to initialise random with (e.g. 1D pixel index).
 * @returns The 2D new stratified sampler.
 */
StratifiedSampler2D MakeStratifiedSampler2D(uint seed)
{
    StratifiedSampler2D ret =
    {
        0,
        seed,
        0,
    };
    return ret;
}

/**
 * Initialise a 2D sequence stratified sample generator from an dimensional stratified sampler.
 * This can be used to branch off from an existing StratifiedSampler and locally generate additional samples
 *  from within the same dimension.
 * @param strat The dimensional sampler to initialise from.
 * @param offset (Optional) The number of values expected to be taken with this sampler that is used to offset the start index accordingly.
 * @returns The new 2D stratified sampler.
 */
StratifiedSampler2D MakeStratifiedSampler2D(StratifiedSampler strat, uint offset = 0)
{
    StratifiedSampler2D ret =
    {
        strat.index * offset,
        strat.seed,
        strat.dimension,
    };
    return ret;
}

#endif // STRATIFIED_SAMPLER_HLSL
