/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2014, Numenta, Inc.  Unless you have an agreement
 * with Numenta, Inc., for a separate license for this software code, the
 * following terms and conditions apply:
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 *
 * http://numenta.org/licenses/
 * ---------------------------------------------------------------------
 */

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <vector>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <nupic/algorithms/SpatialPooler.hpp>
#include <nupic/math/SparseMatrix.hpp>
#include <nupic/math/SparseMatrixProto.capnp.h>
#include <nupic/utils/ProtoUtils.hpp>
#include <nupic/utils/Random.hpp>

using namespace std;
using namespace nupic;
using namespace nupic::algorithms::spatial_pooler;

long diff(timeval & start, timeval & end)
{
  return (
      ((end.tv_sec - start.tv_sec) * 1000000) +
      (end.tv_usec - start.tv_usec)
  );
}

void testSP()
{
  Random random(10);
  struct timeval start, end;

  const UInt inputSize = 500;
  const UInt numColumns = 500;
  const UInt w = 50;

  vector<UInt> inputDims{inputSize};
  vector<UInt> colDims{numColumns};

  SpatialPooler sp1;
  sp1.initialize(inputDims, colDims);

  UInt input[inputSize];
  for (UInt i = 0; i < inputSize; ++i)
  {
    if (i < w)
    {
      input[i] = 1;
    } else {
      input[i] = 0;
    }
  }
  UInt output[numColumns];

  for (UInt i = 0; i < 10000; ++i)
  {
    random.shuffle(input, input + inputSize);
    sp1.compute(input, true, output, false);
  }

  // Now we reuse the last input to test after serialization

  vector<UInt> activeColumnsBefore;
  for (UInt i = 0; i < numColumns; ++i)
  {
    if (output[i] == 1)
    {
      activeColumnsBefore.push_back(i);
    }
  }

  // Save initial trained model
  ofstream osA("outA.proto", ofstream::binary);
  sp1.write(osA);
  osA.close();

  ofstream osC("outC.proto", ofstream::binary);
  sp1.save(osC);
  osC.close();

  SpatialPooler sp2;

  long timeA, timeC = 0;

  for (UInt i = 0; i < 100; ++i)
  {
    // Create new input
    random.shuffle(input, input + inputSize);

    // Get expected output
    UInt outputBaseline[numColumns];
    sp1.compute(input, true, outputBaseline, false);

    UInt outputA[numColumns];
    UInt outputC[numColumns];

    // A - First do iostream version
    {
      SpatialPooler spTemp;

      gettimeofday(&start, nullptr);

      // Deserialize
      ifstream is("outA.proto", ifstream::binary);
      spTemp.read(is);
      is.close();

      // Feed new record through
      spTemp.compute(input, true, outputA, false);

      // Serialize
      ofstream os("outA.proto", ofstream::binary);
      spTemp.write(os);
      os.close();

      gettimeofday(&end, nullptr);
      timeA = timeA + diff(start, end);
    }
    // C - Next do old version
    {
      SpatialPooler spTemp;

      gettimeofday(&start, nullptr);

      // Deserialize
      ifstream is("outC.proto", ifstream::binary);
      spTemp.load(is);
      is.close();

      // Feed new record through
      spTemp.compute(input, true, outputC, false);

      // Serialize
      ofstream os("outC.proto", ofstream::binary);
      spTemp.save(os);
      os.close();

      gettimeofday(&end, nullptr);
      timeC = timeC + diff(start, end);
    }

    for (UInt i = 0; i < numColumns; ++i)
    {
      NTA_ASSERT(outputBaseline[i] == outputA[i]);
      NTA_ASSERT(outputBaseline[i] == outputC[i]);
    }
  }

  remove("outA.proto");
  remove("outC.proto");

  cout << "Time for iostream capnp: " << ((Real)timeA / 1000.0) << endl;
  cout << "Time for old method: " << ((Real)timeC / 1000.0) << endl;
}

void testRandomIOStream(UInt n)
{
  Random r1(7);
  Random r2;

  struct timeval start, end;
  gettimeofday(&start, nullptr);
  for (UInt i = 0; i < n; ++i)
  {
    r1.getUInt32();

    // Serialize
    ofstream os("random2.proto", ofstream::binary);
    r1.write(os);
    os.flush();
    os.close();

    // Deserialize
    ifstream is("random2.proto", ifstream::binary);
    r2.read(is);
    is.close();

    // Test
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
  }
  gettimeofday(&end, nullptr);

  remove("random2.proto");

  cout << "Stream time: " << ((Real)diff(start, end) / 1000.0) << endl;
}

void testRandomManual(UInt n)
{
  Random r1(7);
  Random r2;

  struct timeval start, end;
  gettimeofday(&start, nullptr);
  for (UInt i = 0; i < n; ++i)
  {
    r1.getUInt32();

    // Serialize
    ofstream os("random3.proto", ofstream::binary);
    os << r1;
    os.flush();
    os.close();

    // Deserialize
    ifstream is("random3.proto", ifstream::binary);
    is >> r2;
    is.close();

    // Test
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
    NTA_ASSERT(r1.getUInt32() == r2.getUInt32());
  }
  gettimeofday(&end, nullptr);

  remove("random3.proto");

  cout << "Manual time: " << ((Real)diff(start, end) / 1000.0) << endl;
}

int main(int argc, const char * argv[])
{
  UInt n = 1000;
  testRandomIOStream(n);
  testRandomManual(n);

  testSP();

  return 0;
}
