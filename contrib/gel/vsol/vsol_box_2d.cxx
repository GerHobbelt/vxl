// This is gel/vsol/vsol_box_2d.cxx
#include "vsol_box_2d.h"
//:
// \file

vsol_box_2d::vsol_box_2d()
{
  // vcl_cout << "making a boxology object\n";
}

vsol_box_2d::vsol_box_2d(vsol_box_2d const &b){
  box_ = b.box_;
}
vsol_box_2d::~vsol_box_2d()
{
  // vcl_cout << "deleting a boxology object\n";
}

double vsol_box_2d::get_min_x()
{
  return (box_.min())[0];
}

double vsol_box_2d::get_max_x()
{
  return (box_.max())[0];
}

double vsol_box_2d::get_min_y()
{
  return (box_.min())[1];
}

double vsol_box_2d::get_max_y()
{
  return (box_.max())[1];
}


void vsol_box_2d::set_min_x(const double& v)
{
  (box_.min())[0]=v;
}

void vsol_box_2d::set_max_x(const double& v)
{
  (box_.max())[0]=v;
}

void vsol_box_2d::set_min_y(const double& v)
{
  (box_.min())[1]=v;
}

void vsol_box_2d::set_max_y(const double& v)
{
  (box_.max())[1]=v;
}

// compare mins and maxs between this and the comp_box, grow to the bounding box
void vsol_box_2d::grow_minmax_bounds(vsol_box_2d & comp_box)
{
  if (comp_box.get_min_x() < get_min_x()) set_min_x(comp_box.get_min_x());
  if (comp_box.get_max_x() > get_max_x()) set_max_x(comp_box.get_max_x());
  if (comp_box.get_min_y() < get_min_y()) set_min_y(comp_box.get_min_y());
  if (comp_box.get_max_y() > get_max_y()) set_max_y(comp_box.get_max_y());
}

//-------------------------------------------------------------------
//:   Determines if this box is inside the right hand side box.
//    That is, all boundaries of *this must be on or inside the boundaries of b.
bool vsol_box_2d::operator< (vsol_box_2d& b)
{
  return 
    this->get_min_x() >= b.get_min_x() &&
    this->get_min_y() >= b.get_min_y() &&
    this->get_max_x() <= b.get_max_x() &&
    this->get_max_y() <= b.get_max_y();
}

inline static bool near_same(double f1, double f2, float tolerance)
{
  return f1-f2<tolerance && f2-f1<tolerance;
}

bool vsol_box_2d::near_equal(vsol_box_2d& b, float tolerance)
{
  return 
    near_same(this->get_min_x(), b.get_min_x(), tolerance) &&
    near_same(this->get_min_y(), b.get_min_y(), tolerance) &&
    near_same(this->get_max_x(), b.get_max_x(), tolerance) &&
    near_same(this->get_max_y(), b.get_max_y(), tolerance);
}
void vsol_box_2d::reset_bounds()
{
  for(int i =0; i<2; i++)
    {
      (box_.min())[i]=0;
      (box_.max())[i]=0;
    }
}
