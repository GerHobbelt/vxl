// This is brl/bseg/sdet/sdet_edgel_regions.cxx
#include "sdet_edgel_regions.h"
//:
// \file

#include <vcl_iostream.h>
#include <vcl_cstdlib.h> // for vcl_abs(int)
#include <vcl_cassert.h>
#include <vcl_algorithm.h> // vcl_find()
#include <vbl/vbl_qsort.h> //for sorting labels
#include <vdgl/vdgl_intensity_face.h>

#include <vil/vil_byte.h>
#include <vil/vil_memory_image_of.h>
#include <vcl_cmath.h>     // for sqrt()
#include <vul/vul_timer.h>

#include <vsol/vsol_box_2d.h>
#include <vtol/vtol_edge.h>
#include <vtol/vtol_edge_2d.h>
#include <vtol/vtol_one_chain.h>
#include <vtol/vtol_cycle_processor.h>

#include <vdgl/vdgl_digital_curve.h>
#include <vdgl/vdgl_edgel_chain.h>
#include <vdgl/vdgl_interpolator.h>
#include <btol/btol_vertex_algs.h>
#include <btol/btol_edge_algs.h>

#define bytePixel(buf,x,y)   (*((unsigned char*)buf->GetElementAddr(x,y)))
#define shortPixel(buf,x,y)  (*((short*)buf->GetElementAddr(x,y)))

#if 0 // not ported to vxl
//----------------------------------------------------------------
//: cache debug info
void edgel_regions::cache_bad_edges(CoolArrayP<Edge*>& bad_edges)
{
  vcl_vector<Edge*> corrupt_edges;
  for (CoolArrayP<Edge*>::iterator eit = bad_edges.begin();
       eit != bad_edges.end(); eit++)
    corrupt_edges.push_back(*eit);
  corrupt_edges_.push_back(corrupt_edges);
}
void edgel_regions::cache_bad_edges(vcl_vector<Edge*>& bad_edges)
{
  vcl_vector<Edge*> corrupt_edges;
  for (vcl_vector<Edge*>::iterator eit = bad_edges.begin();
       eit != bad_edges.end(); eit++)
    corrupt_edges.push_back(*eit);
  corrupt_edges_.push_back(corrupt_edges);
}
void edgel_regions::cache_bad_verts(CoolArrayP<Vertex*>& bad_verts)
{
  vcl_vector<Vertex*> corrupt_verts;
  for (CoolArrayP<Vertex*>::iterator vit = bad_verts.begin();
       vit != bad_verts.end(); vit++)
    corrupt_verts.push_back(*vit);
  corrupt_vertices_.push_back(corrupt_verts);
}
#endif

//Print the region label array
void sdet_edgel_regions::print_region_array()
{
  vcl_cout << vcl_endl << vcl_endl;
  for (unsigned int y = yo_; y<=yend_; y++)
    {
      //      vcl_cout << setw(2);
      for (unsigned int x = xo_; x<=xend_; x++)
        if (region_label_array_[Y(y)][X(x)]==EDGE
            //&&edge_boundary_array_[Y(y)][X(x)]->IsVertex()
           )
          vcl_cout << "* " ;
        else
          vcl_cout << region_label_array_[Y(y)][X(x)] << " " ;
      vcl_cout << vcl_endl;
    }
  vcl_cout << vcl_endl << vcl_endl;
}

//Print the contents of the forward eqivalence index
void sdet_edgel_regions::print_region_equivalence()
{
  vcl_cout << vcl_endl << "Label Equivalence:\n"
           << "----------------\n";
  vcl_map<unsigned int, vcl_vector<unsigned int>*>::iterator rpf_iterator;
  for (rpf_iterator= region_pairs_forward_.begin();
       rpf_iterator!=region_pairs_forward_.end(); rpf_iterator++)
    {
      vcl_cout << (*rpf_iterator).first << " == ";
      vcl_vector<unsigned int>* labels = (*rpf_iterator).second;
      if (labels)
        {
          for (vcl_vector<unsigned int>::iterator lit = labels->begin();
               lit != labels->end(); lit++)
            vcl_cout << *lit << " " ;
          vcl_cout << vcl_endl;
        }
    }
  vcl_cout << vcl_endl;
}
//
//Print the contents of the reverse eqivalence index
void sdet_edgel_regions::print_reverse_region_equivalence()
{
  vcl_cout << "\nReverse Label Equivalence:\n"
           << "----------------\n";
  vcl_map<unsigned int, vcl_vector<unsigned int>*>::iterator rpf_iterator;
  for (rpf_iterator= region_pairs_reverse_.begin();
       rpf_iterator!=region_pairs_reverse_.end(); rpf_iterator++)
    {
      vcl_cout << (*rpf_iterator).first << " == ";
      vcl_vector<unsigned int>* labels = (*rpf_iterator).second;
      if (labels)
        {
          for (vcl_vector<unsigned int>::iterator lit = labels->begin();
               lit != labels->end(); lit++)
            vcl_cout << *lit << " ";
          vcl_cout << vcl_endl;
        }
    }
  vcl_cout << vcl_endl;
}
//
//Print the reduced equivalence relation
void sdet_edgel_regions::print_base_equivalence()
{
  vcl_cout << "\nBase Label Equivalence:\n"
           << "----------------\n";

  for (unsigned int i = min_region_label_; i<max_region_label_; i++)
    vcl_cout << i << " == "
             << this->BaseLabel(i) << vcl_endl;
}

//Print the fitted intensity data for all faces
void sdet_edgel_regions::print_intensity_data()
{
  for (vcl_vector<vdgl_intensity_face_sptr>::iterator fit =faces_->begin();
       fit != faces_->end(); fit++)
    {
#if 0
      vdgl_intensity_face* f = *fit;
      vcl_cout << "IntFaceAt(" << f->Xo() << " " << f->Yo() << "):\n";
      f->PrintFit();
#endif
    }
}

//--------------------------------------------------------------
//: The region array can be offset from the corner of the ROI.
//    The following two methods transform from the ROI coordinate
//    system to the region label array coordinate system
unsigned int sdet_edgel_regions::X(unsigned int x)
{
  return x-xo_;
}
unsigned int sdet_edgel_regions::Y(unsigned int y)
{
  return y-yo_;
}
//---------------------------------------------------------
//: Casts the float x location of a point to an unsigned-int location
unsigned int sdet_edgel_regions::Xf(float x)
{
  unsigned int xu = ((unsigned int)(x));
  return xu;
}
//---------------------------------------------------------
//: Casts the float x location of a point to an unsigned-int location
unsigned int sdet_edgel_regions::Yf(float y)
{
  unsigned int yu = ((unsigned int)(y));
  return yu;
}
//----------------------------------------------------------
//: Default constructor
sdet_edgel_regions::sdet_edgel_regions(bool debug, bool verbose)
{
  debug_ = debug;
  verbose_ = verbose;
#if 0
  if (debug_)
    debug_data_ = new topo_debug_data;
  else
    debug_data_ = 0;
#endif
  image_source_ = false;
  buf_source_ = false;
  buf_ = NULL;
  edge_boundary_array_ = NULL;
  region_label_array_ = NULL;
  xo_=0;
  yo_=0;
  xend_ = 0;
  yend_ = 0;
  min_region_label_ = LABEL;
  max_region_label_ = LABEL;
  faces_ = new vcl_vector<vdgl_intensity_face_sptr>;
  face_edge_index_ = NULL;
  intensity_face_index_ = NULL;
  failed_insertions_ = new vcl_vector<vtol_edge_2d_sptr>;
  ubuf_ = NULL;
  sbuf_ = NULL;
}

//----------------------------------------------------------
//: Default destructor
sdet_edgel_regions::~sdet_edgel_regions()
{
  unsigned int y;
  if (region_label_array_)
    for (y=yo_; y<=yend_; y++)
      delete [] region_label_array_[Y(y)];
  delete [] region_label_array_;

#if 0
  for (region_pairs_reverse_.reset(); region_pairs_reverse_.next();)
    delete region_pairs_reverse_.value();

  for (region_pairs_forward_.reset(); region_pairs_forward_.next();)
    delete region_pairs_forward_.value();
  for (equivalence_set_.reset(); equivalence_set_.next();)
    delete equivalence_set_.value();

  for (region_edges_.reset(); region_edges_.next();)
    {
      region_edges_.value()->UnProtect();
    }

  if (edge_boundary_array_)
    for (y = yo_; y<=yend_; y++)
      {
        for (x = xo_; x<=xend_; x++)
          if (edge_boundary_array_[Y(y)][X(x)])
            edge_boundary_array_[Y(y)][X(x)]->UnProtect();
        delete [] edge_boundary_array_[Y(y)];
      }
  delete [] edge_boundary_array_;


  for (region_edge_adjacency_.reset(); region_edge_adjacency_.next();)
    delete region_edge_adjacency_.value();

  for (faces_->reset(); faces_->next();)
    faces_->value()->UnProtect();
  delete faces_;

  if (intensity_face_index_)
    {
      for (unsigned int i = 0; i<max_region_label_; i++)
        if (intensity_face_index_[i])
          intensity_face_index_[i]= NULL;
      delete [] intensity_face_index_;
    }
#endif


  if (face_edge_index_)
    {
      for (unsigned int i = 0; i<max_region_label_; i++)
        delete face_edge_index_[i];
      delete [] face_edge_index_;
    }
  delete failed_insertions_;
  delete [] ubuf_;
  delete [] sbuf_;
}

bool sdet_edgel_regions::compute_edgel_regions(gevd_bufferxy* buf,
                                               vcl_vector<vtol_edge_2d_sptr>& sgrp,
                                               vcl_vector<vdgl_intensity_face_sptr>& faces)
{
  buf_ = buf;
  buf_source_=true;
  image_source_=false;
  return compute_edgel_regions(sgrp, faces);
}

//-----------------------------------------------------------------
//: The key process loop.
//  Carries out the steps:
//   1) Connected components 2)Edge-label assignment 3)Collect region
//   boundaries 4) Construct vdgl_intensity_faces 5)Calculate intensity fit
bool sdet_edgel_regions::
compute_edgel_regions(vil_image& image,
                      vcl_vector<vtol_edge_2d_sptr>& sgrp,
                      vcl_vector<vdgl_intensity_face_sptr>& faces)
{
  image_ = image;
  buf_ = NULL;
  image_source_=true;
  buf_source_ = false;
  return compute_edgel_regions(sgrp, faces);
}

bool sdet_edgel_regions::
compute_edgel_regions(vcl_vector<vtol_edge_2d_sptr>& sgrp,
                      vcl_vector<vdgl_intensity_face_sptr>& faces)
{
  //    corrupt_edges_.clear();
  //    corrupt_vertices_.clear();
#if 0
  if (debug_data_)
    {
      debug_data_->clear();
      if (buf_source_)
        debug_data_->set_buf(buf_);
    }
#endif
  vul_timer t;
  if (!sgrp.size())
    return false;

  //Set up the region label array with edge boundaries
  this->InitRegionArray(sgrp);
  if (verbose_)
    this->print_region_array();
  //Propagate connected components
  unsigned int y, x;
  for (y=yo_; y<yend_; y++)
    for (x=xo_; x<xend_; x++)
      this->UpdateConnectedNeighborhood(X(x), Y(y));
  if (verbose_)
    this->print_region_array();
  //Resolve region label equivalence
  if (verbose_)
    {
      this->print_region_equivalence();
      this->print_reverse_region_equivalence();
    }
  this->GrowEquivalenceClasses();
  this->PropagateEquivalence();
  if (verbose_)
    this->print_base_equivalence();
  this->ApplyRegionEquivalence();
  if (verbose_)
    this->print_region_array();

  //Associate Edge(s) with region labels
  for (y=yo_; y<yend_; y++)
    for (x=xo_; x<xend_; x++)
      this->AssignEdgeLabels(X(x), Y(y));

  //Collect Edge(s) bounding each region
  this->CollectEdges();
  vcl_cout << "Propagate Regions and Collect Edges("
           << max_region_label_-min_region_label_ << ") in "
           << t.real() << " msecs.\n";

  //Collect Face-Edge associations
  this->CollectFaceEdges();

  //Construct vdgl_intensity_faces
  this->ConstructFaces();
  if (!faces_||!faces_->size())
    return false;

  //Collect intensity data for each region
  //this->InsertFaceData();

  //if (verbose_)
    //this->print_intensity_data();
  //Output the result
  faces.clear();
  for (vcl_vector<vdgl_intensity_face_sptr>::iterator fit = faces_->begin();
       fit != faces_->end(); fit++)
    {
      faces.push_back(*fit);
      //        faces_->value()->Protect(); //This caused a big leak
    }
  vcl_cout << "Compute Edgel Regions Total(" << sgrp.size() << ") in "
           << t.real() << " msecs.\n";
  return true;
}


//----------------------------------------------------------
//: assign eqivalence of region label b to region label a
//
bool sdet_edgel_regions::InsertRegionEquivalence(unsigned int label_b,
                                                 unsigned int label_a)
{
  bool result = true;
  result = result && this->add_to_forward(label_b, label_a);
  result = result && this->add_to_reverse(label_a, label_b);
  return result;
}

//--------------------------------------------------------------
//: Get the most basic label equivalent to a given label.
//    If the label_map_ is not defined, this function uses the
//    forward label hash table.  Otherwise 0 is returned.
unsigned int sdet_edgel_regions::BaseLabel(unsigned int label)
{
  vcl_map<unsigned int, unsigned int >::iterator lmtest;
  lmtest = label_map_.find(label);
  if ( lmtest != label_map_.end() )
    return lmtest->second;
  else
    return 0;
}

//------------------------------------------------------------
//:
//  Paint the edgels into the region label array and then
//    output an image where the value is 255 if the pixel is an
//    edge, 0 otherwise
vil_image sdet_edgel_regions::GetEdgeImage(vcl_vector<vtol_edge_2d_sptr>& sg)
{
  if (!this->InitRegionArray(sg)) return NULL;
  int sizex = this->GetXSize(), sizey = this->GetYSize();
  unsigned char no_edge = 0, edge = 255;

  vil_memory_image_of<vil_byte> image(sizex,sizey);

  for (int y = 0; y<sizey; y++)
    for (int x = 0; x<sizex; x++)
      if (region_label_array_[y][x] == EDGE)
        image[y][x] = edge;
      else
        image[y][x] = no_edge;
  return image;
}
//-----------------------------------------------------------
//: Populate the label_map_ to reflect the equivalences between labels.
//
void sdet_edgel_regions::PropagateEquivalence()
{
  vcl_map<unsigned int, vcl_vector<unsigned int>* >::iterator  esi;

  for (esi = equivalence_set_.begin(); esi != equivalence_set_.end(); esi++)
    {
      unsigned int base = esi->first;
      vcl_vector<unsigned int>* labels = esi->second;
      if (!labels) continue;
      int len = labels->size();
      if (!len) continue;
      for (int i = 0; i<len; i++)
        label_map_[(*labels)[i]] = base;
    }
  for (unsigned int i = min_region_label_; i<max_region_label_; i++)
    if (label_map_.find(i) == label_map_.end())
      label_map_[i] = i;
}
//---------------------------------------------------------------------
//:
//  Find the set of labels equivalent to label from a given hash table
//    and merge into equivalence_set_. cur_label is the
//    equivalence set being formed. label is the table key of equivalences
//    to be merged. The labels equivalent to label are in the input map, tab.
bool sdet_edgel_regions::
merge_equivalence(vcl_map<unsigned int, vcl_vector<unsigned int>* >& tab,
                  unsigned int cur_label,
                  unsigned int label)
{
  vcl_map<unsigned int, vcl_vector<unsigned int>* >::iterator hashi;
  //If we can't find the label in tab then there are no equivalences to be merged
  hashi = tab.find(label);
  if (hashi == tab.end()) {
    return false;
  }
  //We did find label, and labels is a set of equivalent labels
  vcl_vector<unsigned int>* labels = hashi->second;

  //If the set is empty then nothing to do
  int len = labels->size();
  if (!len) {
    return false;
  }

  //The set of labels equivalent to cur_label
  vcl_vector<unsigned int>* array = NULL;

  hashi = equivalence_set_.find(cur_label);
  if ( hashi == equivalence_set_.end())
    {
      //We didn't find any equivalent labels for cur_label so we initialize a new one
      //and insert it into equivalence_set
      array = new vcl_vector<unsigned int>;
      equivalence_set_[cur_label] = array;
    }
  else
    {
      //otherwise there is already a growing set of equivalent labels
      array = hashi->second;
    }

  //We look through the set of labels found in map for label
  bool found = false;
  for (int i = 0; i<len; i++)
    {
      unsigned int l = (*labels)[i];
      found = false;
      //see if l is in the current set of equivalent labels for cur_label
      for (unsigned int j=0 ; j < array->size() ; j++)
        {
          if ((*array)[j] == l) found = true;
        }
      //If lable l is not already there then add it
      if (!found)
        {
          array->push_back(l);
        }
    }

  return true;
}
//----------------------------------------------------------------
//: Find the next label not accounted for in the current equivalence set.
//    The set of labels is searched to find a label larger than label, but
//    not in the set, labels.
bool sdet_edgel_regions::get_next_label(vcl_vector<unsigned int>* labels,
                                        unsigned int& label)
{
  //If the set labels is null then
  //just return the next larger label (if less than max_region_label_)
  unsigned int tmp = label+1;
  if (!labels)
    if (tmp<max_region_label_)
      {
        label = tmp;
        return true;
      }

  //The set is not empty, so search for a label not found in the
  //set.

  for (unsigned int i = tmp; i<max_region_label_; i++)
    {
      //if we don't find i we can use it as the new label
      if (vcl_find(labels->begin(), labels->end(), i) == labels->end())
        {
          label = i;
          return true;
        }
    }
  return false;
}
//----------------------------------------------------------------
//:   Form equivalence classes by transitive closure on each label.
//    The resulting label equivalence is stored in the map, equivalence_set_.
//
//    The idea is to add labels to the current equivalence set, cur_set,
//    from either forward or reverse equivalence classes until no
//    new equivalences can be found.  The current equivalence class,
//    cur_set, is repeatedly scanned when new labels are added to pick up
//    new equivalence groups.  Old equivalence entries are removed from
//    forward and reverse forward and reverse equivalence maps as they
//    are used.  When the cur_set is completely closed, i.e. no new labels
//    can be added, then a new label not in cur_set is used to
//    seed a new equivalence class.
//
void sdet_edgel_regions::GrowEquivalenceClasses()
{
  if ((max_region_label_-min_region_label_) < 2)
    return;
  //start with the minimum label
  unsigned int cur_label = min_region_label_;
  //the iterator for equivalence_set_
  vcl_map<unsigned int, vcl_vector<unsigned int>* >::iterator mei;
  while (true)
    {
      bool merging = true;
      vcl_vector<unsigned int>* cur_set = NULL;
      unsigned int i = cur_label;
      int len = 0;
      int old_len = 0;
      while (merging)
        {
          old_len = len;
          merging = false;

          //find label equivalence in the forward map
          bool find_forward =
            this->merge_equivalence(region_pairs_forward_, cur_label, i);
          if (find_forward)
            region_pairs_forward_.erase(i);//remove hits from the forward map

          //find label equivalence in the reverse map
          bool find_reverse =
            this->merge_equivalence(region_pairs_reverse_, cur_label, i);
          if (find_reverse)
            region_pairs_reverse_.erase(i);//remove hits from the reverse map

          //At this point we may have established or added to the equivalence set
          //for cur_label stored in equivalence_set[cur_label]
          //we check if we have
          mei = equivalence_set_.find(cur_label);
          if (mei == equivalence_set_.end()) continue;
          //If we don't find cur_label, it means that we couldn't find label i
          //in either region_pairs_forward or region_pairs_reverse, so we need
          //to get a new i.

          //We did find a growing equivalence set, cur_set, but it might be null
          //or empty
          cur_set = equivalence_set_[cur_label];
          if (!cur_set) continue;
          len = cur_set->size();
          if (!len) continue;

          //Now we check cur_set has actually been extended.
          //If it has we have to sort the labels so that we can find
          //the next larger label in cur_set easily
          if (len > old_len)
            {
              i = cur_label;
              vbl_qsort_ascending(*cur_set);//vcl_sort didn't seem to work!
            }

          //Get the next larger label from cur_set
          //so that we can insert its equivalent labels
          for (int j = 0; j<len&&!merging; j++)
            if ((*cur_set)[j]>i)
              {
                i = (*cur_set)[j];
                merging = true;
              }
          //If we reach here with merging = false then we have finished the
          //current equivalence class
        }
      //Find the next largest label that isn't in cur_set to seed the
      //next equivalence class
      if (!get_next_label(cur_set, cur_label)) return;
    }
}
//------------------------------------------------------------
//: Check if the SpatialGroup contains Edge(s)
//
bool sdet_edgel_regions::GroupContainsEdges(vcl_vector<vtol_edge_2d_sptr>& sg)
{
#if 0
  CoolString type(sg.GetSpatialGroupName());
  return type == CoolString("EdgelGroup") ||
    type == CoolString("FittedEdgeGroup");
#endif
  return true; // TODO
}
//Advance along a line and generate continguous pixels on the line.
//This function assures that the spatial digitization of the Edge(s)
//produces completely connected boundaries.
bool sdet_edgel_regions::line_gen(float xs, float ys, float xe, float ye,
                                  bool& init, bool& done,
                                  unsigned int& x, unsigned int& y)
{
  assert(xs >= 0.0f); assert(ys >= 0.0f);
  const float pix_edge = 1.0f; //We are working at scale = 1.0
  static float xi=0, yi=0;
  if (init)
    {
      xi = xs;
      yi = ys;
      x = (unsigned int)(xi/pix_edge);
      y = (unsigned int)(yi/pix_edge);
      init = false;
      return true;
    }
  if (done) return false;
  float dx = xe-xs;
  float dy = ye-ys;
  float mag = vcl_sqrt(dx*dx + dy*dy);
  if (mag<pix_edge)//Can't reach the next pixel under any circumstances
    {             //so just output the target, xe, ye.
      x = (unsigned int)xe; y = (unsigned int)ye;
      done = true;
      return true;
    }
  float delta = (0.5f*pix_edge)/mag; //move in 1/2 pixel increments
  //Previous pixel location
  int xp = int(xi/pix_edge);
  int yp = int(yi/pix_edge);
  //Increment along the line until the motion is greater than one pixel
  for (int i = 0; i<3; i++)
    {
      xi += dx*delta;
      yi += dy*delta;
      //Check for end of segment, make sure we emit the end of the segment
      if ((xe>=xs&&xi>xe)||(xe<=xs&&xi<xe)||(ye>=ys&&yi>ye)||(ye<=ys&&yi<ye))
        {
          x = (unsigned int)xe; y = (unsigned int)ye;
          done = true;
          return true;
        }
      //Check if we have advanced by more than .5 pixels
      x = (unsigned int)(xi/pix_edge);
      y = (unsigned int)(yi/pix_edge);
      if (vcl_abs(int(x)-xp)>(.5*pix_edge)||vcl_abs(int(y)-yp)>(.5*pix_edge))
        return true;
    }
  vcl_cout << "in sdet_edgel_regions line_gen: - shouldn't happen\n";
  return false;
}
//----------------------------------------------------------
//: A utility for inserting an edgel into the region_label_array_.
//    An edgel and a previous edgel in the chain are used to interpolate
//    intermediate edgels to take account of pixel quantization
bool sdet_edgel_regions::insert_edgel(float pre_x, float pre_y,
                                      float xedgel, float yedgel, sdet_region_edge* re)
{
  unsigned int xinterp, yinterp;
  bool edge_insert = false;
  bool init = true, done = false;
  while (this->line_gen(pre_x, pre_y, xedgel, yedgel, init, done, xinterp, yinterp))
    {
      if (out_of_bounds(X(xinterp), Y(yinterp)))
        {
          vcl_cout << "In sdet_edgel_regions::insert_edgel - out of bounds "
                   << "at (" << xinterp << " " << yinterp << ")\n";
          continue;
        }

      region_label_array_[Y(yinterp)][X(xinterp)] = EDGE;

      if (!re) continue;
      sdet_region_edge* old_re = edge_boundary_array_[Y(yinterp)][X(xinterp)];
      if (old_re&&old_re->get_edge())
        {
          //        old_re->UnProtect();
          edge_boundary_array_[Y(yinterp)][X(xinterp)] = re;
          //      re->Protect();
          edge_insert = true;
        }
      if (!old_re)
        {
          edge_boundary_array_[Y(yinterp)][X(xinterp)] = re;
          //      re->Protect();
          edge_insert = true;
        }
    }
  return edge_insert;
}

int sdet_edgel_regions::bytes_per_pix()
{
  int bypp = 1;
  if (image_source_)
    {
      int bytes_per_comp = (image_.bits_per_component()+7)/8;
      bypp = image_.components() * bytes_per_comp;
    }
  if (buf_source_)
    bypp = buf_->GetBytesPixel();
  return bypp;
}
//-----------------------------------------------------------
//: Initialize the region label array.
//  There are three types of region label symbols:
//  1) UNLABELED - no label has yet been assigned,
//  2) EDGE, the existence of an edgel boundary pixel.
//  3) An unsigned integer which represents an existing region label.

bool sdet_edgel_regions::InitRegionArray(vcl_vector< vtol_edge_2d_sptr>& sg)
{
  if (!this->GroupContainsEdges(sg))
    return false;
  vsol_box_2d b = btol_edge_algs::bounding_box(sg);
  //Get the bounds for the arrays from the input edges
  xo_ = (unsigned int)b.get_min_x();
  yo_ = (unsigned int)b.get_min_y();

  xend_ = (unsigned int)b.get_max_x();
  yend_ = (unsigned int)b.get_max_y();

  unsigned int sizex = xend_ - xo_ + 1;
  unsigned int sizey = yend_ - yo_ + 1;
  //Construct the arrays
  edge_boundary_array_ = new sdet_region_edge**[sizey];
  unsigned int y;
  for (y = yo_; y<=yend_; y++)
    edge_boundary_array_[Y(y)] = new sdet_region_edge*[sizex];

  region_label_array_ = new unsigned int*[sizey];
  for (y = yo_; y<=yend_; y++)
    region_label_array_[Y(y)] = new unsigned int[sizex];

  switch(this->bytes_per_pix())
    {
    case 1:     // Grey scale image with one byte per pixel
      ubuf_ = new unsigned char[sizex];
      break;
    case 2:     // Grey scale image with an unsigned short per pixel
      {
        sbuf_ = new unsigned short[sizex];
        break;
      }
    default:
      vcl_cout<<"In vdgl_intensity_face::get_intensity(): bytes/pixel not 1 or 2\n";
    }

  //Intialize the arrays
  unsigned int x;
  if (ubuf_)
    for (x = xo_; x<=xend_; x++)
      ubuf_[X(x)] = 0;

  if (sbuf_)
    for (x = xo_; x<=xend_; x++)
      sbuf_[X(x)] = 0;

  for (y = yo_; y<=yend_; y++)
    for (x = xo_; x<=xend_; x++)
      {
        region_label_array_[Y(y)][X(x)] = UNLABELED;
        edge_boundary_array_[Y(y)][X(x)] = NULL;
      }
  //Insert edgels into arrays.

  int counter=0;
  for (vcl_vector<vtol_edge_2d_sptr >::iterator sgit = sg.begin();
       sgit != sg.end(); sgit++)
    {
      vtol_edge_2d_sptr e = (*sgit);
      if (!e)
        continue;
      e->set_id(counter++);
      vdgl_digital_curve* dc = e->curve()->cast_to_digital_curve();
      if (!dc)
        continue;
      //The sdet_region_edge enables the link between a region label and
      //an Edge from the input segmentation
      sdet_region_edge* re = new sdet_region_edge(e);
      region_edges_[e->get_id()] = re;
      //      e->Protect(); re->Protect();
      //      float * ex = dc->GetX();
      //      float * ey = dc->GetY();
      vdgl_edgel_chain_sptr xy= dc->get_interpolator()->get_edgel_chain();
      int n_edgels = xy->size();

      //Insert the vertices at the ends of the edge
      //If there is a gap between the vertex locations and the
      //nearest edgels, the insert_edgel function will generate
      //edge insertions to prevent any gaps in the region boundary
      //There shouldn't be DigitalCurve(s) with this defect but it
      //does seem to occur.
      sdet_region_edge* vre = new sdet_region_edge(NULL);
      float pex1, pey1, pex2, pey2;
      if (n_edgels>0)
        {
          pex1 = float((*xy)[0].x()); pex2 = float((*xy)[n_edgels-1].x());
          pey1 = float((*xy)[0].y()); pey2 = float((*xy)[n_edgels-1].y());
          this->insert_edgel(pex1, pey1,
                             float(e->v1()->cast_to_vertex_2d()->x()),
                             float(e->v1()->cast_to_vertex_2d()->y()),
                             vre);
          this->insert_edgel(pex2, pey2,
                             float(e->v2()->cast_to_vertex_2d()->x()),
                             float(e->v2()->cast_to_vertex_2d()->y()),
                             vre);
        }
      else
        {
          pex1 = float(e->v1()->cast_to_vertex_2d()->x());
          pey1 = float(e->v1()->cast_to_vertex_2d()->y());
          pex2 = float(e->v2()->cast_to_vertex_2d()->x());
          pey2 = float(e->v2()->cast_to_vertex_2d()->y());
          this->insert_edgel(pex1, pey1, pex1, pey1, vre);
          this->insert_edgel(pex1, pey1, pex2, pey2, vre);
        }
      //Insert the edgels between the vertices
      //An edgel cannot be inserted on top of an existing vertex
      //insertion.  A vertex is distinguished by having a NULL Edge.
      //That is, for vertices it is ambiguous what Edge instance
      //exists at that location. Edge insertion fails if no edgels
      //of the Edge can be sucessfully inserted.
      bool edge_insert = false;
      for (int k =0; k < n_edgels; k++)
        {
          bool inserted = this->insert_edgel(pex1, pey1, float((*xy)[k].x()), float((*xy)[k].y()), re);
          edge_insert = edge_insert || inserted;
          pex1 = float((*xy)[k].x());
          pey1 = float((*xy)[k].y());
        }
      if (!edge_insert)
        {
          vcl_cout << "Edge Insert Failed for (" << e->v1() << " "
                   << e->v2() << ")N: "<< n_edgels << vcl_endl;
          failed_insertions_->push_back(e);
        }
    }
  vcl_cout << "region_edges size after InitRegionArray = " << region_edges_.size()<< vcl_endl;
  return true;
}

//---------------------------------------------------------------
//: Get code for a given label.
//
unsigned char sdet_edgel_regions::label_code(unsigned int label)
{
  unsigned char result = 0;
  if (label<min_region_label_)
    result = label;
  else
    result = LABEL;
  return result;
}
//----------------------------------------------------------------
//: Add a new pair to the forward equivalence list.
//    That is, a ==> b. Note that there can be multiple equivalences which are
//    stored in a vcl_vector
bool sdet_edgel_regions::add_to_forward(unsigned int key, unsigned int value)
{
  bool result = true;
  vcl_map<unsigned int, vcl_vector<unsigned int>* >::iterator rpfi;
  rpfi = region_pairs_forward_.find(key);

  if (rpfi !=region_pairs_forward_.end())
    {
      vcl_vector<unsigned int> * vec = region_pairs_forward_[key];
      bool found = false;
      for (unsigned int i =0 ; i < vec->size() ; i++)
        {
          if ((*vec)[i] == value)
            found = true;
        }
      if (!found)
        {
          vec->push_back(value);
        }
    }
  else
    {
      vcl_vector<unsigned int>* larray = new vcl_vector<unsigned int>;
      larray->push_back(value);
      region_pairs_forward_[key]=larray;
    }
  return result;
}

//----------------------------------------------------------------
//: Add a new pair to the reverse equivalence list.
//    That is, b==>a. Note that there can be multiple equivalences which are
//    stored in a vcl_vector
//
bool sdet_edgel_regions::add_to_reverse(unsigned int key, unsigned int value)
{
  bool result = true;
  vcl_map<unsigned int, vcl_vector<unsigned int>* >::iterator rpfi;
  rpfi = region_pairs_reverse_.find(key);

  if (rpfi !=region_pairs_reverse_.end())
    {
      vcl_vector<unsigned int> * vec = region_pairs_reverse_[key];
      bool found = false;
      for (unsigned int i =0 ; i < vec->size() ; i++)
        {
          if ((*vec)[i] == value)
            found = true;
        }
      if (!found)
        {
          vec->push_back(value);
        }
    }
  else
    {
      vcl_vector<unsigned int>* larray = new vcl_vector<unsigned int>;
      larray->push_back(value);
      region_pairs_reverse_[key]=larray;
    }
  return result;
}
//-------------------------------------------------------------------
//: Encode a 2x2 neighbor hood with the state of the region array for a given location.
//    The Neighborhood    The states are:
//         ul ur          UNLABELED, EDGE, LABEL
//         ll lr              0       1      2
//    The encoding maps the 2x2 pattern to an unsigned char.  The layout
//    of the uchar is:  [ul|ur|ll|lr] with 2 bits for the state of each
//    position.
unsigned char
sdet_edgel_regions::EncodeNeighborhood(unsigned int ul, unsigned int ur,
                                       unsigned int ll, unsigned int lr)
{
  unsigned char nhood = 0, nul = 0, nur = 0, nll = 0, nlr =0;

  nul = label_code(ul);
  nul = nul<<6;
  nhood |= nul;

  nur = label_code(ur);
  nur = nur<<4;
  nhood |= nur;

  nll = label_code(ll);
  nll = nll << 2;
  nhood |= nll;

  nlr = label_code(lr);
  nhood |= nlr;

  return nhood;
}
//----------------------------------------------------------------------
// --This is the fundamental assignment of label equivalence
//
void sdet_edgel_regions::insert_equivalence(unsigned int ll, unsigned int ur, unsigned int& lr)
{
  if (ll!=ur)//Is a=b?
    {//No. We want the smallest label to be the equivalence base
      //Also we don't want to loose earlier equivalences
      unsigned int smaller_label, larger_label;
      //Get the ordering right
      if (ll>ur)
        {smaller_label = ur; larger_label = ll;}
      else
        {smaller_label = ll; larger_label = ur;}
      this->add_to_forward(larger_label, smaller_label);
      this->add_to_reverse(smaller_label, larger_label);
      lr = smaller_label;
    }
  else //yes, a=b the simple case
    lr = ur;
}
//-------------------------------------------------------------------
//: Propagate connected components.
//    Uses an unsigned char encoding
//    a 2x2 neighborhood to propagate region labels. For example:
//    aa ->   aa
//    xx ->   aa
//    Here the lower two labels are set to the upper label.
//    This method operates directly on the region_label_array_.
//
//    Note that the 2x2 neighborhood is encoded as uchar = [ul|ur|ll|lr]
//                                                          2  2  2  2  bits
void sdet_edgel_regions::UpdateConnectedNeighborhood(unsigned int x, unsigned int y)

{
  unsigned int ul = region_label_array_[y][x];
  unsigned int ur = region_label_array_[y][x+1];
  unsigned int& ll = region_label_array_[y+1][x];
  unsigned int& lr = region_label_array_[y+1][x+1];

  unsigned char nhood = this->EncodeNeighborhood(ul, ur, ll, lr);
  switch(int(nhood))
    {
    case 0:
      //xx xx
      //xx xx
      return;
    case 1:
      //xx xx
      //xe xe
      return;
    case 5:
      //xx xx
      //ee ee
      return;

    case 17:
      //xe xe
      //xe xe
      return;

    case 20:
      //xe xe
      //ex ea
      lr = max_region_label_++;
      return;

    case 21:
      //xe xe
      //ee ee
      return;

    case 22:
      //xe xe
      //ea ea
      return;

    case 68:
      //ex ex
      //ex ex
      return;

    case 72:
      //ex ex
      //ax ax
      return;

    case 133:
      //ax aa
      //ee ee
      return;

    case 136:
      //ax ax
      //ax ax
      return;

    case 160:
      //aa aa
      //xx aa
      ll = ul;
      lr = ul;
      return;

    case 161:
      //aa aa
      //xe ae
      ll = ul;
      return;

    case 168:
      //aa aa
      //ax aa
      insert_equivalence(ll, ur, lr);
      return;

    case 169:
      //aa aa
      //ae ae
      return;

    case 164:
      //aa aa
      //ex ea
      lr = ul;
      return;

    case 165:
      //aa aa
      //ee ee
      return;

    case 144:
      //ae ae
      //xx aa
      ll = ul;
      lr = ul;
      return;

    case 145:
      //ae ae
      //xe ae
      ll = ul;
      return;

    case 152:
      //ae ae
      //ax aa
      lr = ul;
      return;

    case 153:
      //ae ae
      //ae ae
      return;

    case 148:
      //ae ae
      //ex eb
      lr = max_region_label_++;
      return;

    case 149:
      //ae ae
      //ee ee
      return;

    case 96:
      //ea ea
      //xx aa
      ll = ur;
      lr = ur;
      return;

    case 97:
      //ea ea
      //xe be
      ll = max_region_label_++;
      return;

    case 104:
      //ea ea
      //bx aa
      insert_equivalence(ll, ur, lr);
      return;

    case 105:
      //ea ea
      //ae ae
      return;

    case 100:
      //ea ea
      //ex ea
      lr = ur;
      return;

    case 101:
      //ea ea
      //ee ee
      return;

    case 80:
      //ee ee
      //xx bb
      ll = max_region_label_++;
      lr = ll;
      return;

    case 81:
      //ee ee
      //xe be
      ll = max_region_label_++;
      return;

    case 88:
      //ee ee
      //ax aa
      lr = ll;
      return;

    case 89:
      //ee ee
      //ae ae
      return;

    case 84:
      //ee ee
      //ex eb
      lr = max_region_label_++;
      return;

    case 85:
      //ee ee
      //ee ee
      return;
    default:
      vcl_cout << "In sdet_edgel_regions::UpdateNeigborhood(..)"
               << "impossible pattern = " << (int)nhood << vcl_endl;
    }
}
static bool reg_edges_neq(sdet_region_edge* r1, sdet_region_edge* r2)
{
  if (!r1||!r2) return false;
  bool v1 = r1->is_vertex();
  bool v2 = r2->is_vertex();
  if (v1||v2) return false;
  return r1->get_edge()!=r2->get_edge();
}
//A collision is defined by the condition where an region is bounded
//by two different edges at adjacent pixels without crossing a vertex.
//This can happen since boundary positions are sub-pixel and region
//definition is at pixel granularity.  The edge collison causes a needed
//edge to be superseeded.
void sdet_edgel_regions::print_edge_colis(unsigned int x, unsigned int y,
                                          sdet_region_edge* r1, sdet_region_edge* r2)
{
  if (reg_edges_neq(r1, r2))
    if (verbose_)
      vcl_cout << "Collision at (" << x+xo_ << " " << y+yo_ << ")\n";
}

//------------------------------------------------------
//:
//  the top of the T is embedded in a boundary.  This routine tests
//  a dangling edge, called "bar" to see if the "T" vertex, "v", joins
//  the contour in a connected chain.  That is, the top of the "T" is
//  part of a chain.  This condition is called "embedded".  This routine
//  is used to remove "hairs" which are extra edges attached to a
//  closed contour by the pixel-level granularity of the region growing
//  process.
static bool embedded_T(vtol_vertex_sptr v, vtol_edge_2d_sptr bar, vcl_vector<vtol_edge_2d_sptr>& real_edges)
{
  bool embedded = true;
  vcl_vector<vtol_edge_sptr>* edges = v->edges();
  int tedges = 0;
  vcl_vector<vtol_edge_sptr>::iterator eit;
  for ( eit = edges->begin(); eit != edges->end(); eit++)
    {
      vtol_edge_2d_sptr e = (*eit)->cast_to_edge_2d();
      if (vcl_find(real_edges.begin(), real_edges.end(), e) == real_edges.end())
        continue;

      if ((*eit)->cast_to_edge_2d()==bar)
        {
          tedges++;
          continue;
        }
      int end_count = (*eit)->other_endpoint(*v)->get_id();
      if (end_count>1)
        {
          tedges++;
          continue;
        }
    }
  delete edges;
  embedded = (tedges>=3);
  return embedded;
}
//--------------------------------------------------------------------
//: Remove hairs from region boundary.
//    This condition can occur because
//    the region labels are not sub-pixel.  A "hair" is an extra edge joined
//    at a vertex which is part of a continuous chain. Unattached vertices
//    are detected by incrementing a count at each vertex for each
//    attached edge in the input array, "edges".  Such hairs are removed from
//    the input array.
bool sdet_edgel_regions::remove_hairs(vcl_vector<vtol_edge_2d_sptr>& edges)
{
  vcl_vector<vtol_edge_2d_sptr> hairs;
  vcl_vector<vtol_edge_2d_sptr> temp;
  //Intialize Markers
  vcl_vector<vtol_edge_2d_sptr>::iterator eit = edges.begin();
  for (;eit != edges.end(); eit++)
    {
      vtol_vertex_sptr v1 = (*eit)->v1();
      vtol_vertex_sptr v2 = (*eit)->v2();
      v1->set_id(0);
      v2->set_id(0);
      temp.push_back(*eit);
    }
  //Use the vertex id as a counter for the number of edges incident
  eit = edges.begin();
  for (;eit != edges.end(); eit++)
    {
      vtol_vertex_sptr v1 = (*eit)->v1();
      vtol_vertex_sptr v2 = (*eit)->v2();
      int id1 = v1->get_id();
      v1->set_id(++id1);
      int id2 = v2->get_id();
      v2->set_id(++id2);
    }
  eit = edges.begin();
  for (;eit != edges.end(); eit++)
    {
      vtol_vertex_sptr v1 = (*eit)->v1();
      vtol_vertex_sptr v2 = (*eit)->v2();
      int id1 = v1->get_id(), id2 = v2->get_id();
      //The definition of a "hair" a dangling edge meeting at a "T"
      if ((id1==1&&id2>2)||(id1>2&&id2==1))
        {
          //Make sure the top of the "T" is connected
          if (id1>2&&embedded_T(v1, *eit, temp))
            hairs.push_back(*eit);
          else
            if (id2>2&&embedded_T(v2, *eit, temp))
              hairs.push_back(*eit);
        }
    }
  for (vcl_vector<vtol_edge_2d_sptr>::iterator hit = hairs.begin();
       hit != hairs.end(); hit++)
    edges.erase(vcl_find(edges.begin(),edges.end(),*hit));

  return hairs.size() != 0;
}
//-------------------------------------------------------------------
//:
//    There can be some gaps in the region boundary due to missing
//    edges caused by a high local density of vertices.  Take each
//    un-attached vertex and look for a match in the failed edge insertion
//    array. If an edge can be attached, do so.
void sdet_edgel_regions::repair_failed_insertions(vcl_vector<vtol_edge_2d_sptr>& edges,
                                                  vcl_vector<vtol_vertex_sptr>& bad_verts)
{
  vcl_vector<vtol_vertex_sptr> temp1, temp2;
  for (vcl_vector<vtol_vertex_sptr>::iterator vit = bad_verts.begin();
       vit != bad_verts.end(); vit++)
    for (vcl_vector<vtol_edge_2d_sptr>::iterator eit = failed_insertions_->begin();
         eit != failed_insertions_->end(); eit++)
      if ((*vit)==(*eit)->v1())
        {
          if (vcl_find(edges.begin(), edges.end(), *eit) != edges.end())
            edges.push_back(*eit);
          temp1.push_back(*vit);
          temp2.push_back((*eit)->v2());
        }
      else if ((*vit)==(*eit)->v2())
        {
          if (vcl_find(edges.begin(), edges.end(), *eit) != edges.end())
            edges.push_back(*eit);
          temp1.push_back(*vit);
          temp2.push_back((*eit)->v1());
        }
  for (vcl_vector<vtol_vertex_sptr>::iterator wit = temp1.begin();
       wit != temp1.end(); wit++)
        bad_verts.erase(wit);

  for (vcl_vector<vtol_vertex_sptr>::iterator vvit = temp2.begin();
       vvit != temp2.end(); vvit++)
    bad_verts.push_back(*vvit);
}

//-------------------------------------------------------------------
//:
//    After connected components have been generated pass over the
//    array and assign region labels to the sdet_region_edge(s).
//    As in ::UpdateConnectedNeighborhood, the algorithm uses a 2x2 neigborhood
//    E.G.,
//    ee But the purpose is to assign labels. No updating of the
//    aa region_label_array_ is carried out.
//
//    Note that the 2x2 neighborhood is encoded as uchar = [ul|ur|ll|lr]
//                                                          2  2  2  2  bits
void sdet_edgel_regions::AssignEdgeLabels(unsigned int x, unsigned int y)

{
  unsigned int ul = region_label_array_[y][x];
  unsigned int ur = region_label_array_[y][x+1];
  unsigned int& ll = region_label_array_[y+1][x];
  unsigned int& lr = region_label_array_[y+1][x+1];
  sdet_region_edge* rul = edge_boundary_array_[y][x];
  sdet_region_edge* rur = edge_boundary_array_[y][x+1];
  sdet_region_edge* rll = edge_boundary_array_[y+1][x];
  sdet_region_edge* rlr = edge_boundary_array_[y+1][x+1];

  unsigned char nhood = this->EncodeNeighborhood(ul, ur, ll, lr);
  switch(int(nhood))
    {
    case 170:
      //aa
      //aa
      return;

    case 169:
      //aa
      //ae
      rlr->Prop(rlr, ul, max_region_label_);
      return;

    case 166:
      //aa
      //ea
      rll->Prop(rll, ul, max_region_label_);
      return;

    case 165:
      //aa
      //ee
      rll->Prop(rll, ul, max_region_label_);
      print_edge_colis(x, y, rll, rlr);
      return;

    case 154:
      //ae
      //aa
      rur->Prop(rur, ul, max_region_label_);
      return;

    case 153:
      //ae
      //ae
      rlr->Prop(rur, ul, max_region_label_);
      print_edge_colis(x, y, rur, rlr);
      return;

    case 150:
      //ae
      //ea
      rur->Prop(rll, ul, max_region_label_);
      rur->Prop(rll, lr, max_region_label_);
      print_edge_colis(x, y, rur, rll);
      return;

    case 149:
      //ae
      //ee
      rll->Prop(rll, ul, max_region_label_);
      rur->Prop(rlr, ul, max_region_label_);
      if (!(rlr->is_vertex()))
        {
          print_edge_colis(x, y, rur, rlr);
          print_edge_colis(x, y, rll, rur);
          print_edge_colis(x, y, rll, rlr);
        }
      return;

    case 106:
      //ea
      //aa
      rul->Prop(rul, ll, max_region_label_);
      return;

    case 105:
      //ea
      //ae
      rlr->Prop(rul, ll, max_region_label_);
      rlr->Prop(rul, ur, max_region_label_);
      print_edge_colis(x, y, rul, rlr);
      return;

    case 102:
      //ea
      //ea
      rll->Prop(rul, ur, max_region_label_);
      print_edge_colis(x, y, rul, rll);
      return;

    case 101:
      //ea
      //ee
      if (!(rll->is_vertex()))
        {
          print_edge_colis(x, y, rul, rll);
          print_edge_colis(x, y, rul, rlr);
          print_edge_colis(x, y, rll, rlr);
        }
      return;

    case 90:
      //ee
      //aa
      rul->Prop(rul,ll, max_region_label_);
      print_edge_colis(x, y, rul, rur);
      return;

    case 89:
      //ee
      //ae
      rul->Prop(rul, ll, max_region_label_);
      rlr->Prop(rur, ll, max_region_label_);
      if (!(rur->is_vertex()))
        {
          print_edge_colis(x, y, rul, rur);
          print_edge_colis(x, y, rul, rlr);
          print_edge_colis(x, y, rur, rlr);
        }
      return;

    case 86:
      //ee
      //ea
      rul->Prop(rul, lr, max_region_label_);
      if (!(rul->is_vertex()))
        {
          print_edge_colis(x, y, rul, rur);
          print_edge_colis(x, y, rul, rll);
          print_edge_colis(x, y, rur, rll);
        }
      return;
    case 85:
      //ee
      //ee
      if (!((rul->is_vertex())||(rur->is_vertex())||
            (rll->is_vertex())||(rlr->is_vertex())))
        {
          print_edge_colis(x, y, rul ,rur);
          print_edge_colis(x, y, rul ,rll);
          print_edge_colis(x, y, rul ,rlr);
          print_edge_colis(x, y, rur ,rll);
          print_edge_colis(x, y, rur ,rlr);
          print_edge_colis(x, y, rll ,rlr);
        }
      return;
    default:
      vcl_cout << "In sdet_edgel_regions::UpdateNeigborhood(..)"
               << "impossible pattern = " << (int)nhood << vcl_endl;
    }
}
//---------------------------------------------------------------------
//: Scan the region_label_array_ and apply the region equivalence map.
//    The result is that all equivalences are reconciled with the smallest
//    labels.
void sdet_edgel_regions::ApplyRegionEquivalence()
{
  unsigned int x, y;
  for (y = yo_; y<=yend_; y++)
    for (x = xo_; x<=xend_; x++)
      {
        //Update the region label array
        unsigned int label = region_label_array_[Y(y)][X(x)];
        if (label<min_region_label_)
          continue;
        unsigned int base = this->BaseLabel(label);
        region_label_array_[Y(y)][X(x)] = base;
      }
}
//-------------------------------------------------------------------
//: Bounds check on region_label_array_
bool sdet_edgel_regions::out_of_bounds(unsigned int x, unsigned int y)
{
  bool out = x>(xend_-xo_)||y>(yend_-yo_);
  return out;
}

//: Get the a region label for an edge used to construct the boundaries.
//   A return corresponding to UNLABELED means the domain outside the ROI
//   or nr is larger than the number of adjacent regions.
unsigned int sdet_edgel_regions::GetLabel(vtol_edge_2d_sptr e, unsigned int nr)
{
  vcl_map<int,sdet_region_edge *>::iterator reit = region_edges_.find(e->get_id());
  if ( reit == region_edges_.end())
    return UNLABELED;
  return reit->second->GetLabel(nr, max_region_label_);
}

//--------------------------------------------------------------------
//: Insert an Edge into the adjacency list for a region
//
void sdet_edgel_regions::insert_adjacency(unsigned int r, vtol_edge_2d_sptr e)
{
  if (!e) return;
  //  e->Protect();
  vcl_map<unsigned int,vcl_vector<vtol_edge_2d_sptr> *>::iterator reit =region_edge_adjacency_.find(r);
  if (reit == region_edge_adjacency_.end())
    {
      vcl_vector<vtol_edge_2d_sptr>* array = new vcl_vector<vtol_edge_2d_sptr>;
      array->push_back(e);
      region_edge_adjacency_[r] = array;
    }
  else
    reit->second->push_back(e);
  //    region_edge_adjacency_.value()->push(e);
}
//------------------------------------------------------------------
//: Get the edges adjacent to each region
//
void sdet_edgel_regions::CollectEdges()
{
  vcl_cout << "region_edges size in CollectEdges = " << region_edges_.size()<< vcl_endl;
  for ( vcl_map<int, sdet_region_edge*>::iterator reit= region_edges_.begin();
        reit != region_edges_.end(); reit++)
    {
      sdet_region_edge* re = reit->second;
      vtol_edge_2d_sptr e = re->get_edge();
      if (verbose_)
        vcl_cout << "\nEdge:" << e << "(" << e->v1() <<  " " << e->v2() <<"):(";
      for (unsigned int i = 0; i<re->NumLabels(max_region_label_); i++)
        {
          unsigned int l = re->GetLabel(i,max_region_label_);
          if (verbose_)
            vcl_cout << "l[" << i << "]:" << l << " ";
          if (l!=0)
            insert_adjacency(l, e);
        }
      if (verbose_)
        vcl_cout << ")\n";
    }
}
//---------------------------------------------------------------------
//: Trace through the topology network keeping regions on the left.
//    At this point, we have a list of edges for each region. We will
//    trace out the list and form OneCycle(s).  Then the set of OneCycle(s)
//    will form the boundary of the face corresponding to the region.
void sdet_edgel_regions::CollectFaceEdges()
{
  vul_timer t;
  unsigned int i;
  vcl_cout<<"Constructing Face-Edges:";

  face_edge_index_ = new vcl_vector<vtol_edge_2d_sptr>*[max_region_label_];
  for (i=0; i<max_region_label_; i++)
    face_edge_index_[i] = NULL;

  for (i =min_region_label_; i<max_region_label_; i++)
    {
      vcl_map<unsigned int, vcl_vector<vtol_edge_2d_sptr>* >::iterator  reait;
      reait = region_edge_adjacency_.find(i);
      if (reait == region_edge_adjacency_.end())
        continue;
      vcl_vector<vtol_edge_2d_sptr>* edges = reait->second;

      int len = edges->size();
      if (!len)
        continue;

      this->remove_hairs(*edges);
      vcl_vector<vtol_vertex_sptr> bad_verts;
      //If the input edge list is corrupt, then attempt to fix it.
      if (vtol_cycle_processor::corrupt_boundary(*edges, bad_verts))
        {
          this->repair_failed_insertions(*edges, bad_verts);
          if (!vtol_cycle_processor::connect_paths(*edges, bad_verts))
            {
              if (verbose_)
                {
                  vcl_cout << "Region [" << i << "] is corrupt\n";
                  vcl_cout << "Bad Vertices\n";
                  for (vcl_vector<vtol_vertex_sptr>::iterator vit = bad_verts.begin();
                       vit != bad_verts.end(); vit++)
                    if (!(*vit)->get_user_flag(VSOL_FLAG1))
                      vcl_cout << *(*vit);
                  for (vcl_vector<vtol_edge_2d_sptr>::iterator eit = edges->begin();
                       eit != edges->end(); eit++)
                    vcl_cout << "\nEdge(\n" << *((*eit)->v1()) << *((*eit)->v2()) <<")\n";
                }
#if 0
              if (debug_data_)
                {
                  debug_data_->set_verts(bad_verts);
                  debug_data_->set_edges(*edges);
                }
#endif
            }
        }
      if (verbose_)
        vcl_cout << " Building Region [" << i << "]\n";
      len = edges->size();

      vcl_vector<vtol_edge_2d_sptr> EdgeSet;
      for (int j =0; j<len; j++)
        {
          vtol_edge_2d_sptr e = (*edges)[j];
          if (verbose_)
            vcl_cout << "Edge(" << e->v1() <<  " " << e->v2() << vcl_endl;
          EdgeSet.push_back ( e );
        }
      vcl_vector<vtol_edge_2d_sptr>* edge_list = new vcl_vector<vtol_edge_2d_sptr>;
      for (vcl_vector<vtol_edge_2d_sptr>::iterator esit = EdgeSet.begin();
           esit != EdgeSet.end() ; esit++)
        edge_list->push_back (*esit );
      face_edge_index_[i] = edge_list;
    }

  vcl_cout << vcl_endl;
  vcl_cout << "Constructed Face-Edges(" << max_region_label_ - min_region_label_
           << ") in " << t.real() << " msecs.\n";
}
//----------------------------------------------------------------
//: Construct Face(s) from Edge(s) in the face_edge_index_ array.
//    This method has been made virtual so that sub-classes of
//    vdgl_intensity_face can be constructed by sub-classes of sdet_edgel_regions.
void sdet_edgel_regions::ConstructFaces()
{
  vul_timer t;
  unsigned int i;
  vcl_cout<<"Constructing Faces:";
  //Initialize the intensity_face_index_
  intensity_face_index_ = new vdgl_intensity_face_sptr[max_region_label_];
  for (i=0; i<max_region_label_; i++)
    intensity_face_index_[i] = NULL;

  for (i =min_region_label_; i<max_region_label_; i++)
    {
      //Retrieve the face boundary edges
      vcl_vector<vtol_edge_2d_sptr>* edge_list = face_edge_index_[i];
      if (!edge_list||!edge_list->size())
        continue;
      //Make a new vdgl_intensity_face
      vtol_cycle_processor cp(*edge_list);
      vcl_vector<vtol_one_chain_sptr> one_chains;
      cp.nested_one_cycles(one_chains, 0.5);
#if 0
      if (!one_chains.size()&&debug_data_)
        {
          debug_data_->set_edges(*edge_list);
          continue;
        }
#endif
      vdgl_intensity_face_sptr face = new vdgl_intensity_face(one_chains);
      //vdgl_intensity_face_sptr face = new vdgl_intensity_face(edge_list);
      //Check if the Face has valid Edges, since the Face
      //constructor can fail
      //looks like an expensive call
      vcl_vector<vtol_edge_sptr>* face_edges = face->edges();
      if (face_edges->size())
        {
          faces_->push_back(face);
          //  face->Protect();
          intensity_face_index_[i] = face;
          //  face->Protect();
        }
      //      else
      //        face->UnProtect();
      delete face_edges;
    }
  vcl_cout << "\nConstructed Faces(" << max_region_label_ - min_region_label_
           << ") in " << t.real() << " msecs.\n";
}
//--------------------------------------------------------------
//: get a row from a BufferXY
//
void sdet_edgel_regions::get_buffer_row(unsigned int row)
{
  if (!buf_source_)
    return;
  int x0 = (int)xo_, y0 = (int)row, xsize = (int)(xend_-xo_ + 1);
  for (int x = x0; x<xsize; x++)
    switch(this->bytes_per_pix())
      {
      case 1:   // Grey scale image with one byte per pixel
        ubuf_[x]= bytePixel(buf_, x, y0);
        break;
      case 2:   // Grey scale image with an unsigned short per pixel
        sbuf_[x] = shortPixel(buf_, x, y0);
        break;

      default:
        vcl_cout<<"In sdet_edgel_rgions::get_row(): bytes/pixel not 1 or 2\n";
        return;
      }
}
//These routines were added to speed up the intensity face data fill
//------------------------------------------------
//: Get an image row
void sdet_edgel_regions::get_image_row(unsigned int row)
{
  if (!image_source_)
    return;
  int x0 = (int)xo_, y0 = (int)row, xsize = (int)(xend_-xo_ + 1);

  switch (this->bytes_per_pix())
    {
    case 1:     // Grey scale image with one byte per pixel
      image_.get_section(ubuf_, x0, y0, xsize, 1);
      break;
    case 2:     // Grey scale image with an unsigned short per pixel
      image_.get_section(sbuf_, x0, y0, xsize, 1);
      break;

    default:
      vcl_cout<<"In sdet_edgel_regions::get_row(): bytes/pixel not 1 or 2\n";
    }
}
//---------------------------------------------------------------
//: Get the intensity of a single pixel
unsigned short sdet_edgel_regions::get_intensity(unsigned int x)
{
  unsigned int intensity = 0;
  switch (this->bytes_per_pix())
    {
    case 1:     // Grey scale image with one byte per pixel
      intensity = (unsigned short)ubuf_[X(x)];
      break;
    case 2:     // Grey scale image with an unsigned short per pixel
      {
        intensity = sbuf_[X(x)];
        break;
      }
    default:
      vcl_cout<<"In sdet_edgel_regions::get_intensity(): bytes/pixel not 1 or 2\n";
    }
  return intensity;
}
//---------------------------------------------------------------------
//: Accumulate intensity statistics from each region and update the vdgl_intensity_face parameters
void sdet_edgel_regions::AccumulateMeans()
{
  vul_timer t;
  unsigned int i =0;
  //Initialize the intensity face means
  for (i=min_region_label_; i<max_region_label_; i++)
    if (intensity_face_index_[i])
      intensity_face_index_[i]->ResetPixelData();

  int Npixels = 0;
  for (unsigned int y=yo_; y<=yend_; y++)
    {
      if (image_source_)
        this->get_image_row(y);
      if (buf_source_)
        this->get_buffer_row(y);
      for (unsigned int x=xo_; x<=xend_; x++)
        {
          unsigned int label = region_label_array_[Y(y)][X(x)];
          Npixels++;
          if (intensity_face_index_[label])
            {
              unsigned short intensity = this->get_intensity(x);
              float Ximg = float(x)+.5f;//coordinates are at the pixel center
              float Yimg = float(y)+.5f;
              intensity_face_index_[label]->
                IncrementMeans(Ximg, Yimg, intensity);
            }
        }
    }
  int Nregions = max_region_label_ - min_region_label_;
  vcl_cout << "Accumulate Region Means(" << Nregions << ") in "
           << t.real() << " msecs.\n";
  vcl_cout << "Normalized Time = " << (10000.0*t.real())/Npixels << vcl_endl;
}

//---------------------------------------------------------------------
//: Insert region pixels into the vdgl_intensity_face arrays
//
void sdet_edgel_regions::AccumulateRegionData()
{
  vul_timer t;
  //Initialize the intensity face pixel arrays
  for (unsigned int i = min_region_label_; i<max_region_label_; i++)
    if (intensity_face_index_[i])
      (intensity_face_index_[i])->InitPixelArrays();
  //Scan the image and insert intensities
  for (unsigned int y=yo_; y<=yend_; y++)
    {
      if (image_source_)
        this->get_image_row(y);
      if (buf_source_)
        this->get_buffer_row(y);
      for (unsigned int x=xo_; x<=xend_; x++)
        {
          unsigned int label = region_label_array_[Y(y)][X(x)];
          if (intensity_face_index_[label])
            {
              unsigned short intensity= this->get_intensity(x);
              //Set face pixel arrays
              vdgl_intensity_face_sptr f = intensity_face_index_[label];
              float Ximg = float(x)+.5f;
              float Yimg = float(y)+.5f;
              f->InsertInPixelArrays(Ximg, Yimg, intensity);
            }
        }
    }
  vcl_cout << "Accumulate Region Data(" << max_region_label_ - min_region_label_
           << ") in " << t.real() << " msecs.\n";
}
//---------------------------------------------------------------------
//:
//    Do both a scatter matrix update and insertion into the region pixel
//    arrays of each intensity face.  AccumulateScatterData is done first
//    so that the number of pixels can be determined.
//
void sdet_edgel_regions::InsertFaceData()
{
  this->AccumulateMeans();
  this->AccumulateRegionData();
}

