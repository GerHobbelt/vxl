#ifndef osl_fit_circle_h_
#define osl_fit_circle_h_

#ifdef __GNUC__
#pragma interface
#endif

// This is osl/osl_fit_circle.h

//:
// \file
// \brief  least-squares fit of N points to a circle
//
// Fits N given points to a circle using SVD.
// Derived from a program originally
// written by Zhen Song and Dr. Chen, Yangquan
//
// error() flag is set, if an error occurs. There's a constructor
// for an edgel chain as well as for an array of points, but
// only the raw points of the edgels will be used.
//
// \author Markus Meyer (meyer@mesw.de)

#include <vcl_list.h>
#include <vnl/vnl_matrix.h>
#include <vgl/vgl_point_2d.h>
#include <osl/osl_edgel_chain.h>

class osl_fit_circle
{
public:
    //: Construct from list of 2d points of double
    osl_fit_circle(const vcl_list<vgl_point_2d<double> > &points);

    //: Construct from edgel chain (use only raw edge coordinates)
    osl_fit_circle(const osl_edgel_chain& chain);

    // Accessors

    //: get center of circle as vgl_point_2d
    const vgl_point_2d<double>& center() const { return center_; }

    //: get radius of circle as double
    double radius() const { return radius_; }

    //: if error() returns true, there was an error during calculation,
    // normally because of wrong or insufficient input data
    bool error() const { return error_; }

    //: returns the maximum difference between the points and the calculated
    // circle (length of longest tangent from point to circle)
    double max_diff() const { return max_diff_; }

    //: return the average difference between the points and the calculated
    // circle (average of length of tangents from points to circle)
    double avg_diff() const { return avg_diff_; }

protected:
    void calculate(const vcl_list<vgl_point_2d<double> > &points);

    bool error_; // error flag
    double max_diff_, avg_diff_;
    double radius_;
    vgl_point_2d<double> center_;
};

#endif // osl_fit_circle_h_
