#include <vcsl/vcsl_geographic.h>

#include <vcsl/vcsl_spheroid.h>

//***************************************************************************
// Constructors/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
// Default constructor
//---------------------------------------------------------------------------
vcsl_geographic::vcsl_geographic(void)
{
  spheroid_=new vcsl_spheroid();
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
vcsl_geographic::~vcsl_geographic()
{
}

//***************************************************************************
// Status report
//***************************************************************************

//---------------------------------------------------------------------------
// Return the spheroid
//---------------------------------------------------------------------------
vcsl_spheroid_sptr vcsl_geographic::spheroid(void) const
{
  return spheroid_;
}

//***************************************************************************
// Status setting
//***************************************************************************

//---------------------------------------------------------------------------
// Set the spheroid
//---------------------------------------------------------------------------
void vcsl_geographic::set_spheroid(vcsl_spheroid &new_spheroid)
{
  spheroid_=&new_spheroid;
}
