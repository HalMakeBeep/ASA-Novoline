#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>  // For memset

// 2D Vector
struct FVector2D {
    double x, y;

    FVector2D() : x(0), y(0) {}
    FVector2D(double _x, double _y) : x(_x), y(_y) {}

    operator bool() const { return x != 0 || y != 0; }
    bool operator==(const FVector2D& v) const { return x == v.x && y == v.y; }
    FVector2D operator+(const FVector2D& v) const { return FVector2D(x + v.x, y + v.y); }
    FVector2D operator-(const FVector2D& v) const { return FVector2D(x - v.x, y - v.y); }
    FVector2D operator*(double s) const { return FVector2D(x * s, y * s); }

    double length() const { return std::sqrt(x * x + y * y); }
    double distance(const FVector2D& v) const { return (*this - v).length(); }
};

// 3D Vector
struct FVector {
    double x, y, z;

    FVector() : x(0), y(0), z(0) {}
    FVector(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}

    operator bool() const { return x != 0 || y != 0 || z != 0; }
    bool operator==(const FVector& v) const { return x == v.x && y == v.y && z == v.z; }
    FVector operator+(const FVector& v) const { return FVector(x + v.x, y + v.y, z + v.z); }
    FVector operator-(const FVector& v) const { return FVector(x - v.x, y - v.y, z - v.z); }
    FVector operator*(double s) const { return FVector(x * s, y * s, z * s); }
    FVector operator/(double s) const { return FVector(x / s, y / s, z / s); }

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double distance(const FVector& v) const { return (*this - v).length(); }
    
    FVector normalized() const {
        double len = length();
        if (len > 0) return *this / len;
        return FVector();
    }
};

// Rotator (Euler angles)
struct FRotator {
    double pitch, yaw, roll;

    FRotator() : pitch(0), yaw(0), roll(0) {}
    FRotator(double p, double y, double r) : pitch(p), yaw(y), roll(r) {}

    operator bool() const { return pitch != 0 || yaw != 0; }
    bool operator==(const FRotator& r) const { return pitch == r.pitch && yaw == r.yaw && roll == r.roll; }
    FRotator operator+(const FRotator& r) const { return FRotator(pitch + r.pitch, yaw + r.yaw, roll + r.roll); }
    FRotator operator-(const FRotator& r) const { return FRotator(pitch - r.pitch, yaw - r.yaw, roll - r.roll); }
};

// 4x4 Matrix
struct FMatrix {
    double m[4][4];

    FMatrix() { memset(m, 0, sizeof(m)); }
};

// Linear Color (RGBA float)
struct FLinearColor {
    float r, g, b, a;

    FLinearColor() : r(0), g(0), b(0), a(0) {}
    FLinearColor(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}

    operator bool() const { return r != 0 || g != 0 || b != 0 || a != 0; }
    bool operator==(const FLinearColor& c) const { return r == c.r && g == c.g && b == c.b && a == c.a; }

    // Common colors
    static FLinearColor Red() { return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); }
    static FLinearColor Green() { return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f); }
    static FLinearColor Blue() { return FLinearColor(0.0f, 0.0f, 1.0f, 1.0f); }
    static FLinearColor Yellow() { return FLinearColor(1.0f, 1.0f, 0.0f, 1.0f); }
    static FLinearColor Orange() { return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); }
    static FLinearColor Purple() { return FLinearColor(0.5f, 0.0f, 1.0f, 1.0f); }
    static FLinearColor White() { return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); }
    static FLinearColor Black() { return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f); }
};

// Minimal View Info (camera)
struct FMinimalViewInfo {
    FVector location;
    FRotator rotation;
    float fov;
};

// Type aliases for backwards compatibility
using fvector = FVector;
using fvector2d = FVector2D;
using frotator = FRotator;
using fmatrix = FMatrix;
using flinearcolor = FLinearColor;
using fminimalviewinfo = FMinimalViewInfo;
