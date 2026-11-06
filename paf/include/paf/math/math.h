/*
	ScePaf-RE Project
*/

#ifndef _PAF_MATH_MATH_H
#define _PAF_MATH_MATH_H

#if defined(__SNC__)

#include <paf/math/ngp/misc.h>
#include <paf/math/ngp/v1.h>
#include <paf/math/ngp/v1_impl.h>
#include <paf/math/ngp/v2.h>
#include <paf/math/ngp/v2_impl.h>
#include <paf/math/ngp/v3.h>
#include <paf/math/ngp/v3_impl.h>
#include <paf/math/ngp/v4.h>
#include <paf/math/ngp/v4_impl.h>
#include <paf/math/ngp/matrix.h>
#include <paf/math/ngp/matrix3x3.h>
#include <paf/math/ngp/quaternion.h>
#include <paf/math/ngp/spherical_coords.h>
#include <paf/math/rect.h>

#elif defined(_WIN32)

#include <paf/math/generic/misc.h>
#include <paf/math/generic/v1.h>
#include <paf/math/generic/v1_impl.h>
#include <paf/math/generic/v2.h>
#include <paf/math/generic/v2_impl.h>
#include <paf/math/generic/v3.h>
#include <paf/math/generic/v3_impl.h>
#include <paf/math/generic/v4.h>
#include <paf/math/generic/v4_impl.h>
#include <paf/math/generic/matrix.h>
#include <paf/math/generic/matrix3x3.h>
#include <paf/math/generic/quaternion.h>
#include <paf/math/generic/spherical_coords.h>
#include <paf/math/rect.h>

#elif defined(__vita__)

#include <paf/math/ngp/misc.h>
#include <psp2/types.h>

namespace paf {
	namespace math {
		class v1 {
		private:
			SceFPlane impl;
		};

		class v2 {
		public:
			v2() : _x(0.0f), _y(0.0f) {}
			v2(float x, float y) : _x(x), _y(y) {}

			bool operator!=(const v2& rh) {
				if (this->_x != rh._x) {
					return true;
				}
				if (this->_y != rh._y) {
					return true;
				}

				return false;
			}

			bool operator==(const v2& rh) {
				return !(*this != rh);
			}

			float extract_x() const { return _x; }
			float extract_y() const { return _y; }
			float x() const { return _x; }
			float y() const { return _y; }
			void set_x(float x) { _x = x; }
			void set_y(float y) { _y = y; }

		private:
			float _x;
			float _y;
		};

		class v3 {
		public:
			bool operator!=(const v3& rh) {
				if (this->_impl.x != rh._impl.x) {
					return true;
				}
				if (this->_impl.y != rh._impl.y) {
					return true;
				}
				if (this->_impl.z != rh._impl.z) {
					return true;
				}
				/*
								if(this->impl.d != rh.impl.d){
									return true;
								}
				*/

				return false;
			}

			bool operator==(const v3& rh) {
				return !(*this != rh);
			}

			float extract_x() const
			{
				return _impl.x;
			}

			float extract_y() const
			{
				return _impl.y;
			}

			float extract_z() const
			{
				return _impl.z;
			}

		private:
			SceFVector4 _impl;
		};

		class v4 {
		public:
			v4() {
				impl.a = 0.0f;
				impl.b = 0.0f;
				impl.c = 0.0f;
				impl.d = 0.0f;
			}

			v4(float val) {
				impl.a = val;
				impl.b = val;
				impl.c = val;
				impl.d = val;
			}

			v4(float a, float b, float c = 0.0f, float d = 0.0f) {
				impl.a = a;
				impl.b = b;
				impl.c = c;
				impl.d = d;
			}

			static v4 _0000() {
				return v4(0.0f, 0.0f, 0.0f, 0.0f);
			}

			bool operator!=(const v4& rh) {
				if (this->impl.a != rh.impl.a) {
					return true;
				}
				if (this->impl.b != rh.impl.b) {
					return true;
				}
				if (this->impl.c != rh.impl.c) {
					return true;
				}
				if (this->impl.d != rh.impl.d) {
					return true;
				}

				return false;
			}

			bool operator==(const v4& rh) {
				return !(*this != rh);
			}

			float extract_x() const { return impl.a; }
			float extract_y() const { return impl.b; }
			float extract_z() const { return impl.c; }
			float extract_w() const { return impl.d; }
			float x() const { return impl.a; }
			float y() const { return impl.b; }
			float z() const { return impl.c; }
			float w() const { return impl.d; }
			void set_x(float x) { impl.a = x; }
			void set_y(float y) { impl.b = y; }
			void set_z(float z) { impl.c = z; }
			void set_w(float w) { impl.d = w; }

		private:
			SceFPlane impl;
		};
		class matrix {
		private:
			SceFMatrix4 impl;
		};
		class quaternion {
		private:
			SceFQuaternion impl;
		};
	}
}

#else
#error Current target platform is not supported or no target platform defined
#endif

#endif /* _PAF_MATH_MATH_H */ 