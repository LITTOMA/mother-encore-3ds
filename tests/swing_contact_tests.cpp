#include "encore/field_swing.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=3)return 2;
 std::string error;FieldSwingData data;if(!data.load_file(argv[1],error))return 3;FILE*f=std::fopen(argv[2],"rb");if(!f)return 4;
 unsigned checks=0,mismatches=0;float px,py,angle,x,y,ex,ey;unsigned expected;int columns;
 while((columns=std::fscanf(f,"%f\t%f\t%f\t%f\t%f\t%f\t%f\t%u",&px,&py,&angle,&x,&y,&ex,&ey,&expected))==8){
  if(expected>1)return 11;
  auto pose=data.initial();pose.pivot=angle;pose.hitbox=true;bool result=false;
  if(!swing_rectangle_overlap(data,pose,{px,py},{x,y},{ex,ey},result,error))return 5;
  ++checks;if(result!=bool(expected)){if(mismatches<20)std::fprintf(stderr,"Contact %.6f rect=(%.6f,%.6f) actual=%u expected=%u\n",angle,x,y,unsigned(result),expected);++mismatches;}
  pose.hitbox=false;result=true;if(!swing_rectangle_overlap(data,pose,{px,py},{x,y},{ex,ey},result,error)||result)return 6;
 }
 if(columns!=EOF||!std::feof(f)||std::fclose(f)||checks!=8766)return 7;
 auto pose=data.initial();pose.hitbox=true;bool out=true;const auto nan=std::numeric_limits<float>::quiet_NaN();FieldSwingData empty;
 if(swing_rectangle_overlap(empty,pose,{0,0},{0,0},{1,1},out,error)||!out)return 8;
 for(Vec2 extents:std::array<Vec2,4>{{{0,1},{1,0},{-1,1},{nan,1}}})if(swing_rectangle_overlap(data,pose,{0,0},{0,0},extents,out,error)||!out)return 9;
 if(swing_rectangle_overlap(data,pose,{nan,0},{0,0},{1,1},out,error)||!out)return 12;
 if(swing_rectangle_overlap(data,pose,{0,0},{0,nan},{1,1},out,error)||!out)return 13;
 pose.pivot=nan;if(swing_rectangle_overlap(data,pose,{0,0},{0,0},{1,1},out,error)||!out)return 10;
 std::printf("Swing contact: %u source geometry queries, %u mismatches; disabled contact and invalid input preserve contract; no Area2D timing claim\n",checks,mismatches);return mismatches?1:0;
}
