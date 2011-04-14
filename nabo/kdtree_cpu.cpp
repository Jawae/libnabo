/*

Copyright (c) 2010, Stephane Magnenat, ASL, ETHZ, Switzerland
You can contact the author at <stephane at magnenat dot net>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETH-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "nabo_private.h"
#include "index_heap.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <queue>
#include <algorithm>
#include <utility>
#include <boost/numeric/conversion/bounds.hpp>
#include <boost/limits.hpp>

/*!	\file kdtree_cpu.cpp
	\brief kd-tree search, cpu implementation
	\ingroup private
*/

namespace Nabo
{
	//! \ingroup private
	//@{
	
	using namespace std;
	
	template<typename T>
	size_t argMax(const typename NearestNeighbourSearch<T>::Vector& v)
	{
		T maxVal(0);
		size_t maxIdx(0);
		for (int i = 0; i < v.size(); ++i)
		{
			if (v[i] > maxVal)
			{
				maxVal = v[i];
				maxIdx = i;
			}
		}
		return maxIdx;
	}
	
	// OPT
	template<typename T, typename Heap>
	pair<T,T> KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<T, Heap>::getBounds(const BuildPointsIt first, const BuildPointsIt last, const unsigned dim)
	{
		T minVal(boost::numeric::bounds<T>::highest());
		T maxVal(boost::numeric::bounds<T>::lowest());
		
		for (BuildPointsCstIt it(first); it != last; ++it)
		{
			const T val(cloud.coeff(dim, *it));
			minVal = min(val, minVal);
			maxVal = max(val, maxVal);
		}
		
		return make_pair(minVal, maxVal);
	}
	
	template<typename T, typename Heap>
	unsigned KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<T, Heap>::buildNodes(const BuildPointsIt first, const BuildPointsIt last, const Vector minValues, const Vector maxValues)
	{
		const int count(last - first);
		const unsigned pos(nodes.size());
		
		//cerr << count << endl;
		if (count == 1)
		{
			nodes.push_back(Node(*first, &cloud.coeff(0, *first)));
			return pos;
		}
		
		// find the largest dimension of the box
		const unsigned cutDim = argMax<T>(maxValues - minValues);
		const T idealCutVal((maxValues(cutDim) + minValues(cutDim))/2);
		
		// get bounds from actual points
		const pair<T,T> minMaxVals(getBounds(first, last, cutDim));
		
		// correct cut following bounds
		T cutVal;
		if (idealCutVal < minMaxVals.first)
			cutVal = minMaxVals.first;
		else if (idealCutVal > minMaxVals.second)
			cutVal = minMaxVals.second;
		else
			cutVal = idealCutVal;
		
		int l(0);
		int r(count-1);
		// partition points around cutVal
		while (1)
		{
			while (l < count && cloud.coeff(cutDim, *(first+l)) < cutVal)
				++l;
			while (r >= 0 && cloud.coeff(cutDim, *(first+r)) >= cutVal)
				--r;
			if (l > r)
				break;
			swap(*(first+l), *(first+r));
			++l; --r;
		}
		const int br1 = l;	// now: points[0..br1-1] < cutVal <= points[br1..count-1]
		r = count-1;
		// partition points[br1..count-1] around cv
		while (1)
		{
			while (l < count && cloud.coeff(cutDim, *(first+l)) <= cutVal)
				++l;
			while (r >= br1 && cloud.coeff(cutDim, *(first+r)) > cutVal)
				--r;
			if (l > r)
				break;
			swap(*(first+l), *(first+r));
			++l; --r;
		}
		const int br2 = l; // now: points[br1..br2-1] == cv < points[br2..count-1]
		
		// find best split index
		int leftCount;
		if (idealCutVal < minMaxVals.first)
			leftCount = 1;
		else if (idealCutVal > minMaxVals.second)
			leftCount = count-1;
		else if (br1 > count / 2)
			leftCount = br1;
		else if (br2 < count / 2)
			leftCount = br2;
		else
			leftCount = count / 2;
		assert(leftCount > 0);
		/*if (leftCount >= count)
		{
			cerr << "Error found in kdtree:" << endl;
			cerr << "cloud size: " << cloud.cols() << endl;
			cerr << "count:" << count << endl;
			cerr << "leftCount: " << leftCount << endl;
			cerr << "br1: " << br1 << endl;
			cerr << "br2: " << br2 << endl;
			cerr << "idealCutVal: " << idealCutVal << endl;
			cerr << "cutVal: " << cutVal << endl;
			cerr << "minMaxVals.first: " << minMaxVals.first << endl;
			cerr << "minMaxVals.second: " << minMaxVals.second << endl;
		}*/
		assert(leftCount < count);
		
		// update bounds for left
		Vector leftMaxValues(maxValues);
		leftMaxValues[cutDim] = cutVal;
		// update bounds for right
		Vector rightMinValues(minValues);
		rightMinValues[cutDim] = cutVal;
		
		// add this
		nodes.push_back(Node(cutDim, cutVal, 0));
		
		// recurse
		const unsigned __attribute__ ((unused)) leftChild = buildNodes(first, first + leftCount, minValues, leftMaxValues);
		assert(leftChild == pos + 1);
		const unsigned rightChild = buildNodes(first + leftCount, last, rightMinValues, maxValues);
		
		// write right child index and return
		nodes[pos].rightChild = rightChild;
		return pos;
	}

	template<typename T, typename Heap>
	KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<T, Heap>::KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt(const Matrix& cloud, const Index dim, const unsigned creationOptionFlags):
		NearestNeighbourSearch<T>::NearestNeighbourSearch(cloud, dim, creationOptionFlags)
	{
		// build point vector and compute bounds
		BuildPoints buildPoints;
		buildPoints.reserve(cloud.cols());
		for (int i = 0; i < cloud.cols(); ++i)
		{
			const Vector& v(cloud.block(0,i,this->dim,1));
			buildPoints.push_back(i);
#ifdef EIGEN3_API
			const_cast<Vector&>(minBound) = minBound.array().min(v.array());
			const_cast<Vector&>(maxBound) = maxBound.array().max(v.array());
#else // EIGEN3_API
			const_cast<Vector&>(minBound) = minBound.cwise().min(v);
			const_cast<Vector&>(maxBound) = maxBound.cwise().max(v);
#endif // EIGEN3_API
		}
		
		// create nodes
		//nodes.resize(getTreeSize(cloud.cols()));
		buildNodes(buildPoints.begin(), buildPoints.end(), minBound, maxBound);
		buildPoints.clear();
		//for (size_t i = 0; i < nodes.size(); ++i)
		//	cout << i << ": " << nodes[i].dim << " " << nodes[i].cutVal <<  " " << nodes[i].rightChild << endl;
		
	}
	
	template<typename T, typename Heap>
	unsigned long KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<T, Heap>::knn(const Matrix& query, IndexMatrix& indices, Matrix& dists2, const Index k, const T epsilon, const unsigned optionFlags, const T maxRadius)
	{
		checkSizesKnn(query, indices, dists2, k);
		
		const bool allowSelfMatch(optionFlags & NearestNeighbourSearch<T>::ALLOW_SELF_MATCH);
		const bool sortResults(optionFlags & NearestNeighbourSearch<T>::SORT_RESULTS);
		const bool collectStatistics(creationOptionFlags & NearestNeighbourSearch<T>::TOUCH_STATISTICS);
		const T maxRadius2(maxRadius * maxRadius);
		assert(nodes.size() > 0);
		
		assert(nodes.size() > 0);
		Heap heap(k);
		
		std::vector<T> off(this->dim, 0);
		
		IndexMatrix result(k, query.cols());
		const int colCount(query.cols());
		unsigned long leafTouchedCount(0);
		for (int i = 0; i < colCount; ++i)
		{
			fill(off.begin(), off.end(), 0);
			heap.reset();
			
			if (allowSelfMatch)
			{
				if (collectStatistics)
					leafTouchedCount += recurseKnn<true, true>(&query.coeff(0, i), 0, 0, heap, off, 1+epsilon, maxRadius2);
				else
					recurseKnn<true, false>(&query.coeff(0, i), 0, 0, heap, off, 1+epsilon, maxRadius2);
			}
			else
			{
				if (collectStatistics)
					leafTouchedCount += recurseKnn<false, true>(&query.coeff(0, i), 0, 0, heap, off, 1+epsilon, maxRadius2);
				else
					recurseKnn<false, false>(&query.coeff(0, i), 0, 0, heap, off, 1+epsilon, maxRadius2);
			}
			
			if (sortResults)
				heap.sort();
			
			heap.getData(indices.col(i), dists2.col(i));
		}
		return leafTouchedCount;
	}
	
	template<typename T, typename Heap> template<bool allowSelfMatch, bool collectStatistics>
	unsigned long KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<T, Heap>::recurseKnn(const T* query, const unsigned n, T rd, Heap& heap, std::vector<T>& off, const T maxError, const T maxRadius2)
	{
		const Node& node(nodes[n]);
		
		if (node.rightChild == Node::INVALID_CHILD)
		{
			//const T dist(dist2<T>(query, cloud.col(index)));
			//const T dist((query - cloud.col(index)).squaredNorm());
			T dist(0);
			const T* qPtr(query);
			const T* dPtr(node.pt);
			for (int i = 0; i < this->dim; ++i)
			{
				const T diff(*qPtr - *dPtr);
				dist += diff*diff;
				qPtr++; dPtr++;
			}
			if ((dist <= maxRadius2) &&
				(dist < heap.headValue()) &&
				(allowSelfMatch || (dist > numeric_limits<T>::epsilon()))
			)
				heap.replaceHead(node.dim, dist);
			return 1;
		}
		else
		{
			unsigned long leafVisitedCount(0);
			const Index cd(node.dim);
			T& offcd(off[cd]);
			//const T old_off(off.coeff(cd));
			const T old_off(offcd);
			const T new_off(query[cd] - node.cutVal);
			if (new_off > 0)
			{
				if (collectStatistics)
					leafVisitedCount += recurseKnn<allowSelfMatch, true>(query, node.rightChild, rd, heap, off, maxError, maxRadius2);
				else
					recurseKnn<allowSelfMatch, false>(query, node.rightChild, rd, heap, off, maxError, maxRadius2);
				rd += - old_off*old_off + new_off*new_off;
				if ((rd <= maxRadius2) &&
					(rd * maxError < heap.headValue()))
				{
					offcd = new_off;
					if (collectStatistics)
						leafVisitedCount += recurseKnn<allowSelfMatch, true>(query, n + 1, rd, heap, off, maxError, maxRadius2);
					else
						recurseKnn<allowSelfMatch, false>(query, n + 1, rd, heap, off, maxError, maxRadius2);
					offcd = old_off;
				}
			}
			else
			{
				if (collectStatistics)
					leafVisitedCount += recurseKnn<allowSelfMatch, true>(query, n+1, rd, heap, off, maxError, maxRadius2);
				else
					recurseKnn<allowSelfMatch, false>(query, n+1, rd, heap, off, maxError, maxRadius2);
				rd += - old_off*old_off + new_off*new_off;
				if ((rd <= maxRadius2) &&
					(rd * maxError < heap.headValue()))
				{
					offcd = new_off;
					if (collectStatistics)
						leafVisitedCount += recurseKnn<allowSelfMatch, true>(query, node.rightChild, rd, heap, off, maxError, maxRadius2);
					else
						recurseKnn<allowSelfMatch, false>(query, node.rightChild, rd, heap, off, maxError, maxRadius2);
					offcd = old_off;
				}
			}
			return leafVisitedCount;
		}
	}
	
	template struct KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<float,IndexHeapSTL<int,float> >;
	template struct KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<float,IndexHeapBruteForceVector<int,float> >;
	template struct KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<double,IndexHeapSTL<int,double> >;
	template struct KDTreeUnbalancedPtInLeavesImplicitBoundsStackOpt<double,IndexHeapBruteForceVector<int,double> >;
	
	//@}
}
