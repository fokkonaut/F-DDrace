
#ifndef BASE_COLOR_H
#define BASE_COLOR_H

#include "vmath.h"

/*
	Title: Color handling
*/

/*
	Function: HueToRgb
		Converts Hue to RGB
*/
inline float HueToRgb(float v1, float v2, float h)
{
	if(h < 0.0f) h += 1;
	if(h > 1.0f) h -= 1;
	if((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
	if((2.0f * h) < 1.0f) return v2;
	if((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f/3.0f) - h) * 6.0f;
	return v1;
}

/*
	Function: HslToRgb
		Converts HSL to RGB
*/
inline vec3 HslToRgb(vec3 HSL)
{
	if(HSL.s == 0.0f)
		return vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l+HSL.s) - (HSL.s*HSL.l);
		float v1 = 2.0f * HSL.l - v2;

		return vec3(HueToRgb(v1, v2, HSL.h + (1.0f/3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f/3.0f)));
	}
}

/*
	Function: HexToRgba
		Converts Hex to Rgba

	Remarks: Hex should be RGBA8
*/
inline vec4 HexToRgba(int hex)
{
	vec4 c;
	c.r = ((hex >> 24) & 0xFF) / 255.0f;
	c.g = ((hex >> 16) & 0xFF) / 255.0f;
	c.b = ((hex >> 8) & 0xFF) / 255.0f;
	c.a = (hex & 0xFF) / 255.0f;

	return c;
}

/*
	Function: HsvToRgb
		Converts Hsv to Rgb
*/
inline vec3 HsvToRgb(vec3 hsv)
{
	int h = int(hsv.x * 6.0f);
	float f = hsv.x * 6.0f - h;
	float p = hsv.z * (1.0f - hsv.y);
	float q = hsv.z * (1.0f - hsv.y * f);
	float t = hsv.z * (1.0f - hsv.y * (1.0f - f));

	vec3 rgb = vec3(0.0f, 0.0f, 0.0f);

	switch(h % 6)
	{
	case 0:
		rgb.r = hsv.z;
		rgb.g = t;
		rgb.b = p;
		break;

	case 1:
		rgb.r = q;
		rgb.g = hsv.z;
		rgb.b = p;
		break;

	case 2:
		rgb.r = p;
		rgb.g = hsv.z;
		rgb.b = t;
		break;

	case 3:
		rgb.r = p;
		rgb.g = q;
		rgb.b = hsv.z;
		break;

	case 4:
		rgb.r = t;
		rgb.g = p;
		rgb.b = hsv.z;
		break;

	case 5:
		rgb.r = hsv.z;
		rgb.g = p;
		rgb.b = q;
		break;
	}

	return rgb;
}

/*
	Function: RgbToHsv
		Converts Rgb to Hsv
*/
inline vec3 RgbToHsv(vec3 rgb)
{
	float h_min = minimum(minimum(rgb.r, rgb.g), rgb.b);
	float h_max = maximum(maximum(rgb.r, rgb.g), rgb.b);

	// hue
	float hue = 0.0f;

	if(h_max == h_min)
		hue = 0.0f;
	else if(h_max == rgb.r)
		hue = (rgb.g-rgb.b) / (h_max-h_min);
	else if(h_max == rgb.g)
		hue = 2.0f + (rgb.b-rgb.r) / (h_max-h_min);
	else
		hue = 4.0f + (rgb.r-rgb.g) / (h_max-h_min);

	hue /= 6.0f;

	if(hue < 0.0f)
		hue += 1.0f;

	// saturation
	float s = 0.0f;
	if(h_max != 0.0f)
		s = (h_max - h_min)/h_max;

	// value
	float v = h_max;

	return vec3(hue, s, v);
}

inline vec3 RgbToLab(vec3 rgb)
{
	vec3 adapt(0.950467f, 1, 1.088969f);
	vec3 xyz(
		0.412424f * rgb.r + 0.357579f * rgb.g + 0.180464f * rgb.b,
		0.212656f * rgb.r + 0.715158f * rgb.g + 0.0721856f * rgb.b,
		0.0193324f * rgb.r + 0.119193f * rgb.g + 0.950444f * rgb.b
	);

#define RGB_TO_LAB_H(VAL) ((VAL > 0.008856f) ? powf(VAL, 0.333333f) : (7.787f*VAL + 0.137931f))

	return vec3(
		116 * RGB_TO_LAB_H( xyz.y / adapt.y) - 16,
		500 * (RGB_TO_LAB_H(xyz.x / adapt.x) - RGB_TO_LAB_H(xyz.y / adapt.y)),
		200 * (RGB_TO_LAB_H(xyz.y / adapt.y) - RGB_TO_LAB_H(xyz.z / adapt.z))
	);

#undef RGB_TO_LAB_H
}

inline float LabDistance(vec3 labA, vec3 labB)
{
	float ld = labA.x - labB.x;
	float ad = labA.y - labB.y;
	float bd = labA.z - labB.z;
	return sqrtf(ld*ld + ad*ad + bd*bd);
}

// Curiously Recurring Template Pattern for type safety
template<typename DerivedT>
class color4_base
{
public:
	union
	{
		float x, r, h;
	};
	union
	{
		float y, g, s;
	};
	union
	{
		float z, b, l, v;
	};
	union
	{
		float w, a;
	};

	constexpr color4_base() :
		x(), y(), z(), a()
	{
	}

	constexpr color4_base(float nx, float ny, float nz, float na) :
		x(nx), y(ny), z(nz), a(na)
	{
	}

	constexpr color4_base(float nx, float ny, float nz) :
		x(nx), y(ny), z(nz), a(1.0f)
	{
	}

	constexpr color4_base(unsigned col, bool alpha = false)
	{
		a = alpha ? ((col >> 24) & 0xFF) / 255.0f : 1.0f;
		x = ((col >> 16) & 0xFF) / 255.0f;
		y = ((col >> 8) & 0xFF) / 255.0f;
		z = ((col >> 0) & 0xFF) / 255.0f;
	}

	// Disallow casting between different instantiations of the color4_base template.
	// The color_cast functions below should be used to convert between colors.
	template<typename OtherDerivedT>
		requires(!std::is_same_v<DerivedT, OtherDerivedT>)
	color4_base(const color4_base<OtherDerivedT> &Other) = delete;

	constexpr float &operator[](int index)
	{
		return ((float *)this)[index];
	}

	constexpr bool operator==(const color4_base &col) const { return x == col.x && y == col.y && z == col.z && a == col.a; }
	constexpr bool operator!=(const color4_base &col) const { return x != col.x || y != col.y || z != col.z || a != col.a; }

	constexpr unsigned Pack(bool Alpha = true) const
	{
		return (Alpha ? ((unsigned)round_to_int(a * 255.0f) << 24) : 0) + ((unsigned)round_to_int(x * 255.0f) << 16) + ((unsigned)round_to_int(y * 255.0f) << 8) + (unsigned)round_to_int(z * 255.0f);
	}

	constexpr unsigned PackAlphaLast(bool Alpha = true) const
	{
		if(Alpha)
			return ((unsigned)round_to_int(x * 255.0f) << 24) + ((unsigned)round_to_int(y * 255.0f) << 16) + ((unsigned)round_to_int(z * 255.0f) << 8) + (unsigned)round_to_int(a * 255.0f);
		return ((unsigned)round_to_int(x * 255.0f) << 16) + ((unsigned)round_to_int(y * 255.0f) << 8) + (unsigned)round_to_int(z * 255.0f);
	}

	constexpr DerivedT WithAlpha(float alpha) const
	{
		DerivedT col(static_cast<const DerivedT &>(*this));
		col.a = alpha;
		return col;
	}

	constexpr DerivedT WithMultipliedAlpha(float alpha) const
	{
		DerivedT col(static_cast<const DerivedT &>(*this));
		col.a *= alpha;
		return col;
	}

	template<typename UnpackT>
	constexpr static UnpackT UnpackAlphaLast(unsigned Color, bool Alpha = true)
	{
		UnpackT Result;
		if(Alpha)
		{
			Result.x = ((Color >> 24) & 0xFF) / 255.0f;
			Result.y = ((Color >> 16) & 0xFF) / 255.0f;
			Result.z = ((Color >> 8) & 0xFF) / 255.0f;
			Result.a = ((Color >> 0) & 0xFF) / 255.0f;
		}
		else
		{
			Result.x = ((Color >> 16) & 0xFF) / 255.0f;
			Result.y = ((Color >> 8) & 0xFF) / 255.0f;
			Result.z = ((Color >> 0) & 0xFF) / 255.0f;
			Result.a = 1.0f;
		}
		return Result;
	}
};

class ColorHSLA : public color4_base<ColorHSLA>
{
public:
	using color4_base::color4_base;
	constexpr ColorHSLA() = default;

	constexpr static const float DARKEST_LGT = 0.5f;
	constexpr static const float DARKEST_LGT7 = 61.0f / 255.0f;

	constexpr ColorHSLA UnclampLighting(float Darkest) const
	{
		ColorHSLA col = *this;
		col.l = Darkest + col.l * (1.0f - Darkest);
		return col;
	}

	constexpr unsigned Pack(bool Alpha = true) const
	{
		return color4_base::Pack(Alpha);
	}

	constexpr unsigned Pack(float Darkest, bool Alpha = false) const
	{
		ColorHSLA col = *this;
		col.l = (l - Darkest) / (1 - Darkest);
		col.l = std::clamp(col.l, 0.0f, 1.0f);
		return col.Pack(Alpha);
	}
};

#endif
