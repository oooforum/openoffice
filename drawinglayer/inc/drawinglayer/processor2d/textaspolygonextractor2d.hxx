/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef INCLUDED_DRAWINGLAYER_PROCESSOR2D_TEXTASPOLYGONEXTRACTOR2D_HXX
#define INCLUDED_DRAWINGLAYER_PROCESSOR2D_TEXTASPOLYGONEXTRACTOR2D_HXX

#include <drawinglayer/drawinglayerdllapi.h>
#include <drawinglayer/processor2d/baseprocessor2d.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/color/bcolor.hxx>
#include <basegfx/color/bcolormodifier.hxx>
#include <vector>

//////////////////////////////////////////////////////////////////////////////

namespace drawinglayer 
{
	namespace processor2d
	{
		/// helper data structure for returning the result
		struct DRAWINGLAYER_DLLPUBLIC TextAsPolygonDataNode
		{
		private:
			basegfx::B2DPolyPolygon					maB2DPolyPolygon;
			basegfx::BColor							maBColor;
			bool									mbIsFilled;

		public:
			TextAsPolygonDataNode(
				const basegfx::B2DPolyPolygon& rB2DPolyPolygon,
				const basegfx::BColor& rBColor,
				bool bIsFilled)
			:	maB2DPolyPolygon(rB2DPolyPolygon),
				maBColor(rBColor),
				mbIsFilled(bIsFilled)
			{
			}

			// data read access
			const basegfx::B2DPolyPolygon& getB2DPolyPolygon() const { return maB2DPolyPolygon; }
			const basegfx::BColor& getBColor() const { return maBColor; }
			bool getIsFilled() const { return mbIsFilled; }
		};

		/// typedef for a vector of that helper data
		typedef ::std::vector< TextAsPolygonDataNode > TextAsPolygonDataNodeVector;
		
        /** TextAsPolygonExtractor2D class

            This processor extracts text in the feeded primitives to filled polygons
         */
		class DRAWINGLAYER_DLLPUBLIC TextAsPolygonExtractor2D : public BaseProcessor2D
		{
		private:
			// extraction target
			TextAsPolygonDataNodeVector				maTarget;

			// the modifiedColorPrimitive stack
			basegfx::BColorModifierStack			maBColorModifierStack;

			// flag if we are in a decomposed text
            sal_uInt32								mnInText;

			// tooling methods
			void processBasePrimitive2D(const primitive2d::BasePrimitive2D& rCandidate);

		public:
			TextAsPolygonExtractor2D(const geometry::ViewInformation2D& rViewInformation);
			virtual ~TextAsPolygonExtractor2D();

			// data read access
			const TextAsPolygonDataNodeVector& getTarget() const { return maTarget; }
		};
	} // end of namespace processor2d
} // end of namespace drawinglayer

//////////////////////////////////////////////////////////////////////////////

#endif // INCLUDED_DRAWINGLAYER_PROCESSOR2D_TEXTASPOLYGONEXTRACTOR2D_HXX

// eof