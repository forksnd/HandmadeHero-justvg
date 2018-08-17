#if !defined(HANDMADE_MATH_H)
#define HANDMADE_MATH_H

struct v2
{
	union
	{	
		struct 
		{
			real32 X, Y;
		};
		real32 E[2];
	};
};

inline v2 
operator*(real32 A, v2 B)
{
	v2 Result;

	Result.X = A * B.X;
	Result.Y = A * B.Y;

	return(Result);
}

inline v2 &
operator*=(v2 &A, real32 B)
{
	A = B * A;

	return(A);
}

inline v2 
operator-(v2 A)
{
	v2 Result;

	Result.X = -A.X;
	Result.Y = -A.Y;

	return(Result);
}

inline v2 
operator+(v2 A, v2 B)
{
	v2 Result;

	Result.X = A.X + B.X;
	Result.Y = A.Y + B.Y;

	return(Result);
}

inline v2 &
operator+=(v2 &A, v2 B)
{
	A = A + B;

	return(A); 
}

inline v2 
operator-(v2 A, v2 B)
{
	v2 Result;

	Result.X = A.X - B.X;
	Result.Y = A.Y - B.Y;

	return(Result);
}

#endif