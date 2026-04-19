/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_VMATH_H
#define BASE_VMATH_H

#include "math.h"

#include <cmath>
#include <cstdint>

// ------------------------------------

template<Numeric T>
class vector2_base
{
public:
	union
	{
		T x, u;
	};
	union
	{
		T y, v;
	};

	constexpr vector2_base() = default;
	constexpr vector2_base(T nx, T ny) :
		x(nx), y(ny)
	{
	}

	constexpr vector2_base operator-() const { return vector2_base(-x, -y); }
	constexpr vector2_base operator-(const vector2_base &vec) const { return vector2_base(x - vec.x, y - vec.y); }
	constexpr vector2_base operator+(const vector2_base &vec) const { return vector2_base(x + vec.x, y + vec.y); }
	constexpr vector2_base operator*(const T rhs) const { return vector2_base(x * rhs, y * rhs); }
	constexpr vector2_base operator*(const vector2_base &vec) const { return vector2_base(x * vec.x, y * vec.y); }
	constexpr vector2_base operator/(const T rhs) const { return vector2_base(x / rhs, y / rhs); }
	constexpr vector2_base operator/(const vector2_base &vec) const { return vector2_base(x / vec.x, y / vec.y); }

	constexpr vector2_base &operator+=(const vector2_base &vec)
	{
		x += vec.x;
		y += vec.y;
		return *this;
	}
	constexpr vector2_base &operator-=(const vector2_base &vec)
	{
		x -= vec.x;
		y -= vec.y;
		return *this;
	}
	constexpr vector2_base &operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		return *this;
	}
	constexpr vector2_base &operator*=(const vector2_base &vec)
	{
		x *= vec.x;
		y *= vec.y;
		return *this;
	}
	constexpr vector2_base &operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		return *this;
	}
	constexpr vector2_base &operator/=(const vector2_base &vec)
	{
		x /= vec.x;
		y /= vec.y;
		return *this;
	}

	constexpr bool operator==(const vector2_base &vec) const { return x == vec.x && y == vec.y; } // TODO: do this with an eps instead
	constexpr bool operator!=(const vector2_base &vec) const { return x != vec.x || y != vec.y; }

	constexpr T &operator[](const int index) { return index ? y : x; }
	constexpr const T &operator[](const int index) const { return index ? y : x; }
};

template<Numeric T>
constexpr vector2_base<T> rotate(const vector2_base<T> &a, float angle)
{
	angle = angle * pi / 180.0f;
	float s = std::sin(angle);
	float c = std::cos(angle);
	return vector2_base<T>(static_cast<T>(c * a.x - s * a.y), static_cast<T>(s * a.x + c * a.y));
}

template<Numeric T>
inline T distance(const vector2_base<T> a, const vector2_base<T> &b)
{
	return length(a - b);
}

template<Numeric T>
inline T distance_squared(const vector2_base<T> &a, const vector2_base<T> &b)
{
	vector2_base<T> v = a - b;
	return v.x * v.x + v.y * v.y;
}

template<Numeric T>
constexpr T dot(const vector2_base<T> a, const vector2_base<T> &b)
{
	return a.x * b.x + a.y * b.y;
}

template<std::floating_point T>
inline float length(const vector2_base<T> &a)
{
	return std::sqrt(dot(a, a));
}

template<std::integral T>
inline float length(const vector2_base<T> &a)
{
	return std::sqrt(static_cast<float>(dot(a, a)));
}

constexpr float length_squared(const vector2_base<float> &a)
{
	return dot(a, a);
}

constexpr float angle(const vector2_base<float> &a)
{
	if(a.x == 0 && a.y == 0)
		return 0.0f;
	else if(a.x == 0)
		return a.y < 0 ? -pi / 2 : pi / 2;
	float result = std::atan(a.y / a.x);
	if(a.x < 0)
		result = result + pi;
	return result;
}

template<Numeric T>
constexpr vector2_base<T> normalize_pre_length(const vector2_base<T> &v, T len)
{
	if(len == 0)
		return vector2_base<T>();
	return vector2_base<T>(v.x / len, v.y / len);
}

inline vector2_base<float> normalize(const vector2_base<float> &v)
{
	float divisor = length(v);
	if(divisor == 0.0f)
		return vector2_base<float>(0.0f, 0.0f);
	float l = 1.0f / divisor;
	return vector2_base<float>(v.x * l, v.y * l);
}

inline vector2_base<float> direction(float angle)
{
	return vector2_base<float>(std::cos(angle), std::sin(angle));
}

inline vector2_base<float> random_direction()
{
	return direction(random_angle());
}

typedef vector2_base<float> vec2;
typedef vector2_base<bool> bvec2;
typedef vector2_base<int> ivec2;

template<Numeric T>
constexpr bool closest_point_on_line(vector2_base<T> line_pointA, vector2_base<T> line_pointB, vector2_base<T> target_point, vector2_base<T> &out_pos)
{
	vector2_base<T> AB = line_pointB - line_pointA;
	T SquaredMagnitudeAB = dot(AB, AB);
	if(SquaredMagnitudeAB > 0)
	{
		vector2_base<T> AP = target_point - line_pointA;
		T APdotAB = dot(AP, AB);
		T t = APdotAB / SquaredMagnitudeAB;
		out_pos = line_pointA + AB * std::clamp(t, (T)0, (T)1);
		return true;
	}
	else
		return false;
}

constexpr int intersect_line_circle(const vec2 LineStart, const vec2 LineEnd, const vec2 CircleCenter, float Radius, vec2 aIntersections[2])
{
	vec2 Delta = LineEnd - LineStart;
	vec2 Offset = LineStart - CircleCenter;

	// A * Time^2 + B * Time + c == 0
	float A = length_squared(Delta);
	float B = 2.0f * dot(Offset, Delta);
	float C = dot(Offset, Offset) - Radius * Radius;

	float Discriminant = B * B - 4.0f * A * C;
	if(Discriminant < 0.0f || A == 0.0f)
	{
		// no intersection
		return 0;
	}
	else if(Discriminant == 0.0f)
	{
		// tangent
		float Time = -B / (2.0f * A);
		aIntersections[0] = LineStart + Delta * Time;
		return 1;
	}
	else
	{
		Discriminant = std::sqrt(Discriminant);
		float Time1 = (-B - Discriminant) / (2.0f * A);
		float Time2 = (-B + Discriminant) / (2.0f * A);

		aIntersections[0] = LineStart + Delta * Time1;
		aIntersections[1] = LineStart + Delta * Time2;

		return 2;
	}
}

template<Numeric T>
inline vector2_base<T> rotate_around_point(vector2_base<T> point, vector2_base<T> pivot, float angle)
{
	T s = sin(angle);
	T c = cos(angle);
	// translate point to origin
	point -= pivot;
	// rotate point
	vector2_base<T> rotated_point;
	rotated_point.x = point.x * c - point.y * s;
	rotated_point.y = point.x * s + point.y * c;
	// translate point back to pivot
	point = rotated_point + pivot;
	return point;
}

// ------------------------------------
template<Numeric T>
class vector3_base
{
public:
	union
	{
		T x, r, h, u;
	};
	union
	{
		T y, g, s, v;
	};
	union
	{
		T z, b, l, w;
	};

	constexpr vector3_base() = default;
	constexpr vector3_base(T nx, T ny, T nz) :
		x(nx), y(ny), z(nz)
	{
	}

	constexpr vector3_base operator-(const vector3_base &vec) const { return vector3_base(x - vec.x, y - vec.y, z - vec.z); }
	constexpr vector3_base operator-() const { return vector3_base(-x, -y, -z); }
	constexpr vector3_base operator+(const vector3_base &vec) const { return vector3_base(x + vec.x, y + vec.y, z + vec.z); }
	constexpr vector3_base operator*(const T rhs) const { return vector3_base(x * rhs, y * rhs, z * rhs); }
	constexpr vector3_base operator*(const vector3_base &vec) const { return vector3_base(x * vec.x, y * vec.y, z * vec.z); }
	constexpr vector3_base operator/(const T rhs) const { return vector3_base(x / rhs, y / rhs, z / rhs); }
	constexpr vector3_base operator/(const vector3_base &vec) const { return vector3_base(x / vec.x, y / vec.y, z / vec.z); }

	constexpr vector3_base &operator+=(const vector3_base &vec)
	{
		x += vec.x;
		y += vec.y;
		z += vec.z;
		return *this;
	}
	constexpr vector3_base &operator-=(const vector3_base &vec)
	{
		x -= vec.x;
		y -= vec.y;
		z -= vec.z;
		return *this;
	}
	constexpr vector3_base &operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return *this;
	}
	constexpr vector3_base &operator*=(const vector3_base &vec)
	{
		x *= vec.x;
		y *= vec.y;
		z *= vec.z;
		return *this;
	}
	constexpr vector3_base &operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		z /= rhs;
		return *this;
	}
	constexpr vector3_base &operator/=(const vector3_base &vec)
	{
		x /= vec.x;
		y /= vec.y;
		z /= vec.z;
		return *this;
	}

	constexpr bool operator==(const vector3_base &vec) const { return x == vec.x && y == vec.y && z == vec.z; } // TODO: do this with an eps instead
	constexpr bool operator!=(const vector3_base &vec) const { return x != vec.x || y != vec.y || z != vec.z; }
};

template<Numeric T>
inline T distance(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return length(a - b);
}

template<Numeric T>
constexpr T dot(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

template<Numeric T>
constexpr vector3_base<T> cross(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return vector3_base<T>(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

//
inline float length(const vector3_base<float> &a)
{
	return std::sqrt(dot(a, a));
}

inline vector3_base<float> normalize(const vector3_base<float> &v)
{
	float divisor = length(v);
	if(divisor == 0.0f)
		return vector3_base<float>(0.0f, 0.0f, 0.0f);
	float l = 1.0f / divisor;
	return vector3_base<float>(v.x * l, v.y * l, v.z * l);
}

typedef vector3_base<float> vec3;
typedef vector3_base<bool> bvec3;
typedef vector3_base<int> ivec3;

// ------------------------------------

template<Numeric T>
class vector4_base
{
public:
	union
	{
		T x, r, h;
	};
	union
	{
		T y, g, s;
	};
	union
	{
		T z, b, l;
	};
	union
	{
		T w, a;
	};

	constexpr vector4_base() = default;
	constexpr vector4_base(T nx, T ny, T nz, T nw) :
		x(nx), y(ny), z(nz), w(nw)
	{
	}

	constexpr vector4_base operator+(const vector4_base &vec) const { return vector4_base(x + vec.x, y + vec.y, z + vec.z, w + vec.w); }
	constexpr vector4_base operator-(const vector4_base &vec) const { return vector4_base(x - vec.x, y - vec.y, z - vec.z, w - vec.w); }
	constexpr vector4_base operator-() const { return vector4_base(-x, -y, -z, -w); }
	constexpr vector4_base operator*(const vector4_base &vec) const { return vector4_base(x * vec.x, y * vec.y, z * vec.z, w * vec.w); }
	constexpr vector4_base operator*(const T rhs) const { return vector4_base(x * rhs, y * rhs, z * rhs, w * rhs); }
	constexpr vector4_base operator/(const vector4_base &vec) const { return vector4_base(x / vec.x, y / vec.y, z / vec.z, w / vec.w); }
	constexpr vector4_base operator/(const T vec) const { return vector4_base(x / vec, y / vec, z / vec, w / vec); }

	constexpr vector4_base &operator+=(const vector4_base &vec)
	{
		x += vec.x;
		y += vec.y;
		z += vec.z;
		w += vec.w;
		return *this;
	}
	constexpr vector4_base &operator-=(const vector4_base &vec)
	{
		x -= vec.x;
		y -= vec.y;
		z -= vec.z;
		w -= vec.w;
		return *this;
	}
	constexpr vector4_base &operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}
	constexpr vector4_base &operator*=(const vector4_base &vec)
	{
		x *= vec.x;
		y *= vec.y;
		z *= vec.z;
		w *= vec.w;
		return *this;
	}
	constexpr vector4_base &operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		z /= rhs;
		w /= rhs;
		return *this;
	}
	constexpr vector4_base &operator/=(const vector4_base &vec)
	{
		x /= vec.x;
		y /= vec.y;
		z /= vec.z;
		w /= vec.w;
		return *this;
	}

	constexpr bool operator==(const vector4_base &vec) const { return x == vec.x && y == vec.y && z == vec.z && w == vec.w; } // TODO: do this with an eps instead
	constexpr bool operator!=(const vector4_base &vec) const { return x != vec.x || y != vec.y || z != vec.z || w != vec.w; }
};

typedef vector4_base<float> vec4;
typedef vector4_base<bool> bvec4;
typedef vector4_base<int> ivec4;
typedef vector4_base<uint8_t> ubvec4;

// Two line segments intersection

// To find orientation of ordered triplet (p, q, r).
// The function returns following values
// 0 --> p, q and r are collinear
// 1 --> Clockwise
// 2 --> Counterclockwise
template<Numeric T>
int orientation(const vector2_base<T> p, const vector2_base<T> q, const vector2_base<T> r)
{
	// See https://www.geeksforgeeks.org/orientation-3-ordered-points/amp/ for details of below formula.
	int val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
	if (val == 0) return 0; // collinear
	return (val > 0) ? 1 : 2; // clock or counterclock wise
}

// Given three collinear points p, q, r, the function checks if
// point q lies on line segment 'pr'
template<Numeric T>
bool on_segment(const vector2_base<T> p, const vector2_base<T> q, const vector2_base<T> r)
{
	return (q.x <= maximum(p.x, r.x) && q.x >= minimum(p.x, r.x) && q.y <= maximum(p.y, r.y) && q.y >= minimum(p.y, r.y));
}

// The main function that returns true if line segment 'p1q1' and 'p2q2' intersect.
template<Numeric T>
inline bool intersect_segments(const vector2_base<T> p1, const vector2_base<T> q1, const vector2_base<T> p2, const vector2_base<T> q2)
{
	// Find the four orientations needed for general and special cases
	int o1 = orientation(p1, q1, p2);
	int o2 = orientation(p1, q1, q2);
	int o3 = orientation(p2, q2, p1);
	int o4 = orientation(p2, q2, q1);

	// General case
	if (o1 != o2 && o3 != o4)
		return true;

	// Special Cases
	// p1, q1 and p2 are collinear and p2 lies on segment p1q1
	if (o1 == 0 && on_segment(p1, p2, q1)) return true;

	// p1, q1 and q2 are collinear and q2 lies on segment p1q1
	if (o2 == 0 && on_segment(p1, q2, q1)) return true;

	// p2, q2 and p1 are collinear and p1 lies on segment p2q2
	if (o3 == 0 && on_segment(p2, p1, q2)) return true;

	// p2, q2 and q1 are collinear and q1 lies on segment p2q2
	if (o4 == 0 && on_segment(p2, q1, q2)) return true;

	// Doesn't fall in any of the above cases
	return false;
}

#endif
