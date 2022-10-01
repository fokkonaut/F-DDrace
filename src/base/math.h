/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <stdlib.h>

template <typename T>
inline T clamp(T val, T min, T max)
{
	if(val < min)
		return min;
	if(val > max)
		return max;
	return val;
}

inline float sign(float f)
{
	return f<0.0f?-1.0f:1.0f;
}

inline int round_to_int(float f)
{
	if(f > 0)
		return (int)(f+0.5f);
	return (int)(f-0.5f);
}

template<typename T, typename TB>
inline T mix(const T a, const T b, TB amount)
{
	return a + (b-a)*amount;
}

template<typename T, typename TB>
inline T bezier(const T p0, const T p1, const T p2, const T p3, TB amount)
{
	// De-Casteljau Algorithm
	const T c10 = mix(p0, p1, amount);
	const T c11 = mix(p1, p2, amount);
	const T c12 = mix(p2, p3, amount);

	const T c20 = mix(c10, c11, amount);
	const T c21 = mix(c11, c12, amount);

	return mix(c20, c21, amount); // c30
}

inline int random_int() { return (((rand() & 0xffff) << 16) | (rand() & 0xffff)) & 0x7FFFFFFF; };
inline float frandom() { return rand()/(float)(RAND_MAX); }
inline int random(int max) { return rand() % max; }
inline int random(int min, int max) { return min + rand() % (max+1 - min); }

// float to fixed
inline int f2fx(float v) { return (int)(v*(float)(1<<10)); }
inline float fx2f(int v) { return v*(1.0f/(1<<10)); }

// int to fixed
inline int i2fx(int v) { return v<<10; }
inline int fx2i(int v) { return v>>10; }

inline int gcd(int a, int b)
{
	while(b != 0)
	{
		int c = a % b;
		a = b;
		b = c;
	}
	return a;
}

class fxp
{
	int value;
public:
	void set(int v) { value = v; }
	int get() const { return value; }
	fxp &operator = (int v) { value = v<<10; return *this; }
	fxp &operator = (float v) { value = (int)(v*(float)(1<<10)); return *this; }
	operator float() const { return value/(float)(1<<10); }
};

const float pi = 3.1415926535897932384626433f;

template <typename T> inline T min(T a, T b) { return a<b?a:b; }
template <typename T> inline T max(T a, T b) { return a>b?a:b; }
template <typename T> inline T absolute(T a) { return a<T(0)?-a:a; }

// Two line segments intersection

// To find orientation of ordered triplet (p, q, r).
// The function returns following values
// 0 --> p, q and r are collinear
// 1 --> Clockwise
// 2 --> Counterclockwise
template<typename T>
int orientation(const T p, const T q, const T r)
{
	// See https://www.geeksforgeeks.org/orientation-3-ordered-points/amp/ for details of below formula.
	int val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
	if (val == 0) return 0; // collinear
	return (val > 0) ? 1 : 2; // clock or counterclock wise
}

// Given three collinear points p, q, r, the function checks if
// point q lies on line segment 'pr'
template<typename T>
bool onSegment(const T p, const T q, const T r)
{
	return (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) && q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y));
}

// The main function that returns true if line segment 'p1q1' and 'p2q2' intersect.
template<typename T>
inline bool intersect_segments(const T p1, const T q1, const T p2, const T q2)
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
	if (o1 == 0 && onSegment(p1, p2, q1)) return true;

	// p1, q1 and q2 are collinear and q2 lies on segment p1q1
	if (o2 == 0 && onSegment(p1, q2, q1)) return true;

	// p2, q2 and p1 are collinear and p1 lies on segment p2q2
	if (o3 == 0 && onSegment(p2, p1, q2)) return true;

	// p2, q2 and q1 are collinear and q1 lies on segment p2q2
	if (o4 == 0 && onSegment(p2, q1, q2)) return true;

	// Doesn't fall in any of the above cases
	return false;
}

#endif // BASE_MATH_H
