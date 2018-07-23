// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#include <cmath>

#include <libcurv/geom/shape.h>
#include <libcurv/exception.h>
#include <libcurv/context.h>
#include <libcurv/gl_context.h>
#include <libcurv/function.h>
#include <libcurv/frame.h>

namespace curv { namespace geom {

Shape_Program::Shape_Program(
    std::unique_ptr<const Context> cx,
    System& sys)
:
    context_(std::move(cx)),
    system_(sys)
{}

BBox
BBox::from_value(Value val, const Context& cx)
{
    auto list = val.to<List>(cx);
    list->assert_size(2, cx);

    At_Index mincx(0, cx);
    auto mins = list->at(0).to<List>(mincx);
    mins->assert_size(3, mincx);

    At_Index maxcx(1, cx);
    auto maxs = list->at(1).to<List>(maxcx);
    maxs->assert_size(3, maxcx);

    BBox b;
    b.xmin = mins->at(0).to_num(cx);
    b.ymin = mins->at(1).to_num(cx);
    b.zmin = mins->at(2).to_num(cx);
    b.xmax = maxs->at(0).to_num(cx);
    b.ymax = maxs->at(1).to_num(cx);
    b.zmax = maxs->at(2).to_num(cx);
    return b;
}

bool
Shape_Program::recognize(Value val)
{
    static Symbol is_2d_key = "is_2d";
    static Symbol is_3d_key = "is_3d";
    static Symbol bbox_key = "bbox";
    static Symbol dist_key = "dist";
    static Symbol colour_key = "colour";

    Value is_2d_val;
    Value is_3d_val;
    Value bbox_val;
    Value dist_val;
    Value colour_val;

    auto s = val.dycast<Structure>();
    if (s == nullptr)
        return false;
    if (s->hasfield(is_2d_key))
        is_2d_val = s->getfield(is_2d_key, *context_);
    else
        return false;
    if (s->hasfield(is_3d_key))
        is_3d_val = s->getfield(is_3d_key, *context_);
    else
        return false;
    if (s->hasfield(bbox_key))
        bbox_val = s->getfield(bbox_key, *context_);
    else
        return false;
    if (s->hasfield(dist_key))
        dist_val = s->getfield(dist_key, *context_);
    else
        return false;
    if (s->hasfield(colour_key))
        colour_val = s->getfield(colour_key, *context_);
    else
        return false;

    is_2d_ = is_2d_val.to_bool(At_Field("is_2d", *context_));
    is_3d_ = is_3d_val.to_bool(At_Field("is_3d", *context_));
    if (!is_2d_ && !is_3d_)
        throw Exception(*context_,
            "at least one of is_2d and is_3d must be true");
    bbox_ = BBox::from_value(bbox_val, At_Field("bbox", *context_));

    dist_fun_ = dist_val.dycast<Function>();
    if (dist_fun_ == nullptr)
        throw Exception(At_Field("dist", *context_), "dist is not a function");
    dist_frame_ = Frame::make(
        dist_fun_->nslots_, system_, nullptr, nullptr, nullptr);

    colour_fun_ = colour_val.dycast<Function>();
    if (colour_fun_ == nullptr)
        throw Exception(At_Field("colour", *context_),
            "colour is not a function");
    colour_frame_ = Frame::make(
        colour_fun_->nslots_, system_, nullptr, nullptr, nullptr);

    return true;
}

struct GL_Data_Ref : public Operation
{
    GL_Value val_;
    GL_Data_Ref(Shared<const Phrase> src, GL_Value v)
    : Operation(std::move(src)), val_(v)
    {}
    GL_Value gl_eval(GL_Frame&) const override { return val_; }
};

GL_Value
Shape_Program::gl_dist(GL_Value arg, GL_Compiler& gl) const
{
    assert(arg.type == GL_Type::Vec4);
    const At_Field cx("dist", *context_);
    auto f = GL_Frame::make(0, gl, &cx, nullptr, nullptr);
    auto aref = make<GL_Data_Ref>(nullptr, arg);
    auto result = dist_fun_->gl_call_expr(*aref, nullptr, *f);
    if (result.type != GL_Type::Num)
        throw Exception(cx, stringify("dist function returns ",result.type));
    return result;
}

GL_Value
Shape_Program::gl_colour(GL_Value arg, GL_Compiler& gl) const
{
    assert(arg.type == GL_Type::Vec4);
    const At_Field cx("colour", *context_);
    auto f = GL_Frame::make(0, gl, &cx, nullptr, nullptr);
    auto aref = make<GL_Data_Ref>(nullptr, arg);
    auto result = colour_fun_->gl_call_expr(*aref, nullptr, *f);
    if (result.type != GL_Type::Vec3)
        throw Exception(cx, stringify("colour function returns ",result.type));
    return result;
}

double
Shape_Program::dist(double x, double y, double z, double t)
{
    Shared<List> point = List::make({Value{x}, Value{y}, Value{z}, Value{t}});
    Value result = dist_fun_->call({point}, *dist_frame_);
    return result.to_num(*context_);
}

Vec3
Shape_Program::colour(double x, double y, double z, double t)
{
    Shared<List> point = List::make({Value{x}, Value{y}, Value{z}, Value{t}});
    Value result = colour_fun_->call({point}, *colour_frame_);
    Shared<List> cval = result.to<List>(*context_);
    cval->assert_size(3, *context_);
    return Vec3{ cval->at(0).to_num(*context_),
                 cval->at(1).to_num(*context_),
                 cval->at(2).to_num(*context_) };
}

}} // namespace
