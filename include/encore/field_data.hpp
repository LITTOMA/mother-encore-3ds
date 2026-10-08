#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// ENCMAP01: one exterior field compiled from a pinned TileMap scene. Schema
// constants and enums only; every coordinate, tile and texture is external data.
enum class FieldMapSection:uint32_t {Bytes=1,Atlases,Frames,Pictures,Variants,Tiles,Shapes,Points,Conditions,Layers,
    Chunks,Cells,Sprites,Items,Groups,Openables,Doors,Boundaries,Notices,Cameras,Bodies};
constexpr uint32_t kFieldMapSectionCount=21;
constexpr uint32_t kFieldNone=0xffffffffu;
constexpr uint16_t kFieldNoOpenable=0xffffu;
struct FieldText {uint32_t offset=0,length=0;};
struct FieldSpan {uint32_t first=0,count=0;};
struct FieldAtlas {FieldText path;uint16_t width=0,height=0;};
struct FieldFrame {uint16_t atlas=0,x=0,y=0,w=0,h=0;};
struct FieldPicture {uint32_t first_frame=0;uint16_t frame_count=0,variant_count=0;float fps=0;uint32_t variant_first=kFieldNone;};
struct FieldVariant {FieldText locale;uint32_t picture=0;};
enum class FieldFlip:uint32_t {Horizontal=1,Vertical=2};
struct FieldTile {uint32_t picture=0;uint16_t w=0,h=0;Vec2 offset{};int32_t z=0;uint32_t shape_first=0,shape_count=0,flags=0;};
enum class FieldShapeKind:uint16_t {Convex=1,Segment=2};
struct FieldShape {uint16_t kind=0,count=0;uint32_t first=0;Vec2 normal{},minimum{},maximum{};};
struct FieldCondition {FieldText flag;bool expected=false;};
enum class FieldLayerFlag:uint32_t {YSort=1,Collides=2,Hidden=4};
struct FieldLayer {FieldText name;uint32_t flags=0;FieldSpan conditions;Vec2 position{};uint32_t chunk_first=0,chunk_count=0;float origin_y=0;};
struct FieldChunk {int16_t cx=0,cy=0;uint32_t first=0,count=0;Vec2 minimum{},maximum{};};
struct FieldCell {int16_t x=0,y=0;uint16_t tile=0,order=0;};
struct FieldSprite {uint32_t picture=0;Vec2 position{};uint16_t w=0,h=0,flags=0,openable=kFieldNoOpenable;FieldSpan conditions;};
// LayerCells: a whole non-y-sorted layer in quadrant order. SortedLayerCells:
// every cell of a y-sorted layer becomes its own sort entry. SortedLayer: a
// non-y-sorted layer drawn whole at one sort key inside a y-sort group.
enum class FieldItemKind:uint16_t {LayerCells=1,SortedLayerCells=2,Sprites=3,Player=4,SortedLayer=5};
struct FieldItem {uint16_t kind=0;uint32_t first=0,count=0;float sort_y=0;uint32_t order=0;FieldSpan conditions;};
enum class FieldGroupKind:uint32_t {Static=1,YSort=2};
struct FieldGroup {uint32_t kind=0,first=0,count=0;};
struct FieldOpenable {FieldText path;Vec2 center{},extents{};FieldText sound,end_sound;float timer=0;uint32_t sprite=0;FieldSpan conditions;};
struct FieldDoor {FieldText path,target;uint32_t route=0;Vec2 center{},extents{};FieldSpan conditions;};
enum class FieldBoundaryKind:uint16_t {LockedDoor=1,Area=2};
struct FieldBoundary {uint16_t kind=0;FieldText path,label;Vec2 center{},extents{};FieldSpan conditions;};
enum class FieldNoticeKind:uint16_t {NotPorted=1,Interaction=2};
struct FieldNotice {uint16_t kind=0;FieldText path,label;Vec2 position{};FieldSpan conditions;};
struct FieldCamera {Vec2 center{},extents{};float left=0,top=0,width=0,height=0;Vec2 offset{};FieldSpan conditions;bool has_rect=false;};
struct FieldBody {uint32_t shape_first=0,shape_count=0;FieldSpan conditions;};

class FieldMapView {
public:
    bool valid()const{return bytes_!=nullptr;}
    explicit operator bool()const{return valid();}
    // Identity of the borrowed buffer, for binding checks between owners.
    const void* bytes_identity()const{return bytes_;}
    uint32_t scene_id()const;
    uint32_t count(FieldMapSection)const;
    std::string_view text(FieldText)const;
    FieldAtlas atlas(uint32_t)const;
    FieldFrame frame(uint32_t)const;
    FieldPicture picture(uint32_t)const;
    FieldVariant variant(uint32_t)const;
    FieldTile tile(uint32_t)const;
    FieldShape shape(uint32_t)const;
    Vec2 point(uint32_t)const;
    FieldCondition condition(uint32_t)const;
    FieldLayer layer(uint32_t)const;
    FieldChunk chunk(uint32_t)const;
    FieldCell cell(uint32_t)const;
    FieldSprite sprite(uint32_t)const;
    FieldItem item(uint32_t)const;
    FieldGroup group(uint32_t)const;
    FieldOpenable openable(uint32_t)const;
    FieldDoor door(uint32_t)const;
    FieldBoundary boundary(uint32_t)const;
    FieldNotice notice(uint32_t)const;
    FieldCamera camera(uint32_t)const;
    FieldBody body(uint32_t)const;
    // Picture for a locale code: an exact reviewed variant, else the default.
    uint32_t localized_picture(uint32_t picture,std::string_view locale)const;
private:
    friend class FieldMapData;
    const uint8_t* record(FieldMapSection,uint32_t)const;
    const uint8_t* bytes_=nullptr;size_t size_=0;
};

// One owned buffer. A rejected load keeps the previous bytes and views valid.
class FieldMapData {
public:
    FieldMapData()=default;
    FieldMapData(const FieldMapData&)=delete;
    FieldMapData& operator=(const FieldMapData&)=delete;
    bool load(const uint8_t*,size_t,std::string&);
    bool load_file(const char*,std::string&);
    FieldMapView view()const{FieldMapView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
    void clear(){bytes_.clear();bytes_.shrink_to_fit();}
private:
    std::vector<uint8_t> bytes_;
};
}
