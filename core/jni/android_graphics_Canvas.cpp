/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jni.h"
#include "GraphicsJNI.h"
#include <android_runtime/AndroidRuntime.h>

#include "Canvas.h"
#include "SkGraphics.h"
#include "SkPorterDuff.h"
#include "TypefaceImpl.h"

#include <minikin/Layout.h>
#include "MinikinSkia.h"
#include "MinikinUtils.h"

namespace android {

namespace CanvasJNI {

static Canvas* get_canvas(jlong canvasHandle) {
    return reinterpret_cast<Canvas*>(canvasHandle);
}

static void finalizer(JNIEnv* env, jobject clazz, jlong canvasHandle) {
    delete get_canvas(canvasHandle);
}

// Native wrapper constructor used by Canvas(Bitmap)
static jlong initRaster(JNIEnv* env, jobject, jlong bitmapHandle) {
    SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    return reinterpret_cast<jlong>(Canvas::create_canvas(bitmap));
}

// Set the given bitmap as the new draw target (wrapped in a new SkCanvas),
// optionally copying canvas matrix & clip state.
static void setBitmap(JNIEnv* env, jobject, jlong canvasHandle, jlong bitmapHandle,
                      jboolean copyState) {
    SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    get_canvas(canvasHandle)->setBitmap(bitmap, copyState);
}

static jboolean isOpaque(JNIEnv*, jobject, jlong canvasHandle) {
    return get_canvas(canvasHandle)->isOpaque() ? JNI_TRUE : JNI_FALSE;
}

static jint getWidth(JNIEnv*, jobject, jlong canvasHandle) {
    return static_cast<jint>(get_canvas(canvasHandle)->width());
}

static jint getHeight(JNIEnv*, jobject, jlong canvasHandle) {
    return static_cast<jint>(get_canvas(canvasHandle)->height());
}

static jint getSaveCount(JNIEnv*, jobject, jlong canvasHandle) {
    return static_cast<jint>(get_canvas(canvasHandle)->getSaveCount());
}

static jint save(JNIEnv*, jobject, jlong canvasHandle, jint flagsHandle) {
    SkCanvas::SaveFlags flags = static_cast<SkCanvas::SaveFlags>(flagsHandle);
    return static_cast<jint>(get_canvas(canvasHandle)->save(flags));
}

static jint saveLayer(JNIEnv* env, jobject, jlong canvasHandle, jfloat l, jfloat t,
                      jfloat r, jfloat b, jlong paintHandle, jint flagsHandle) {
    SkPaint* paint  = reinterpret_cast<SkPaint*>(paintHandle);
    SkCanvas::SaveFlags flags = static_cast<SkCanvas::SaveFlags>(flagsHandle);
    return static_cast<jint>(get_canvas(canvasHandle)->saveLayer(l, t, r, b, paint, flags));
}

static jint saveLayerAlpha(JNIEnv* env, jobject, jlong canvasHandle, jfloat l, jfloat t,
                           jfloat r, jfloat b, jint alpha, jint flagsHandle) {
    SkCanvas::SaveFlags flags = static_cast<SkCanvas::SaveFlags>(flagsHandle);
    return static_cast<jint>(get_canvas(canvasHandle)->saveLayerAlpha(l, t, r, b, alpha, flags));
}

static void restore(JNIEnv* env, jobject, jlong canvasHandle) {
    Canvas* canvas = get_canvas(canvasHandle);
    if (canvas->getSaveCount() <= 1) {  // cannot restore anymore
        doThrowISE(env, "Underflow in restore");
        return;
    }
    canvas->restore();
}

static void restoreToCount(JNIEnv* env, jobject, jlong canvasHandle, jint restoreCount) {
    Canvas* canvas = get_canvas(canvasHandle);
    if (restoreCount < 1 || restoreCount > canvas->getSaveCount()) {
        doThrowIAE(env, "Underflow in restoreToCount");
        return;
    }
    canvas->restoreToCount(restoreCount);
}

static void getCTM(JNIEnv* env, jobject, jlong canvasHandle, jlong matrixHandle) {
    SkMatrix* matrix = reinterpret_cast<SkMatrix*>(matrixHandle);
    get_canvas(canvasHandle)->getMatrix(matrix);
}

static void setMatrix(JNIEnv* env, jobject, jlong canvasHandle, jlong matrixHandle) {
    const SkMatrix* matrix = reinterpret_cast<SkMatrix*>(matrixHandle);
    get_canvas(canvasHandle)->setMatrix(matrix ? *matrix : SkMatrix::I());
}

static void concat(JNIEnv* env, jobject, jlong canvasHandle, jlong matrixHandle) {
    const SkMatrix* matrix = reinterpret_cast<SkMatrix*>(matrixHandle);
    get_canvas(canvasHandle)->concat(*matrix);
}

static void rotate(JNIEnv*, jobject, jlong canvasHandle, jfloat degrees) {
    get_canvas(canvasHandle)->rotate(degrees);
}

static void scale(JNIEnv*, jobject, jlong canvasHandle, jfloat sx, jfloat sy) {
    get_canvas(canvasHandle)->scale(sx, sy);
}

static void skew(JNIEnv*, jobject, jlong canvasHandle, jfloat sx, jfloat sy) {
    get_canvas(canvasHandle)->skew(sx, sy);
}

static void translate(JNIEnv*, jobject, jlong canvasHandle, jfloat dx, jfloat dy) {
    get_canvas(canvasHandle)->translate(dx, dy);
}

static jboolean getClipBounds(JNIEnv* env, jobject, jlong canvasHandle, jobject bounds) {
    SkRect   r;
    SkIRect ir;
    bool result = get_canvas(canvasHandle)->getClipBounds(&r);

    if (!result) {
        r.setEmpty();
    }
    r.round(&ir);

    (void)GraphicsJNI::irect_to_jrect(ir, env, bounds);
    return result ? JNI_TRUE : JNI_FALSE;
}

static jboolean quickRejectRect(JNIEnv* env, jobject, jlong canvasHandle,
                                jfloat left, jfloat top, jfloat right, jfloat bottom) {
    bool result = get_canvas(canvasHandle)->quickRejectRect(left, top, right, bottom);
    return result ? JNI_TRUE : JNI_FALSE;
}

static jboolean quickRejectPath(JNIEnv* env, jobject, jlong canvasHandle, jlong pathHandle) {
    SkPath* path = reinterpret_cast<SkPath*>(pathHandle);
    bool result = get_canvas(canvasHandle)->quickRejectPath(*path);
    return result ? JNI_TRUE : JNI_FALSE;
}

static jboolean clipRect(JNIEnv*, jobject, jlong canvasHandle, jfloat l, jfloat t,
                         jfloat r, jfloat b, jint opHandle) {
    SkRegion::Op op = static_cast<SkRegion::Op>(opHandle);
    bool emptyClip = get_canvas(canvasHandle)->clipRect(l, t, r, b, op);
    return emptyClip ? JNI_FALSE : JNI_TRUE;
}

static jboolean clipPath(JNIEnv* env, jobject, jlong canvasHandle, jlong pathHandle,
                         jint opHandle) {
    SkPath* path = reinterpret_cast<SkPath*>(pathHandle);
    SkRegion::Op op = static_cast<SkRegion::Op>(opHandle);
    bool emptyClip = get_canvas(canvasHandle)->clipPath(path, op);
    return emptyClip ? JNI_FALSE : JNI_TRUE;
}

static jboolean clipRegion(JNIEnv* env, jobject, jlong canvasHandle, jlong deviceRgnHandle,
                           jint opHandle) {
    SkRegion* deviceRgn = reinterpret_cast<SkRegion*>(deviceRgnHandle);
    SkRegion::Op op = static_cast<SkRegion::Op>(opHandle);
    bool emptyClip = get_canvas(canvasHandle)->clipRegion(deviceRgn, op);
    return emptyClip ? JNI_FALSE : JNI_TRUE;
}

static void drawColor(JNIEnv* env, jobject, jlong canvasHandle, jint color, jint modeHandle) {
     SkPorterDuff::Mode mode = static_cast<SkPorterDuff::Mode>(modeHandle);
     get_canvas(canvasHandle)->drawColor(color, SkPorterDuff::ToXfermodeMode(mode));
}

static void drawPaint(JNIEnv* env, jobject, jlong canvasHandle, jlong paintHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawPaint(*paint);
}

static void drawPoint(JNIEnv*, jobject, jlong canvasHandle, jfloat x, jfloat y,
                      jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawPoint(x, y, *paint);
}

static void drawPoints(JNIEnv* env, jobject, jlong canvasHandle, jfloatArray jptsArray,
                       jint offset, jint count, jlong paintHandle) {
    NPE_CHECK_RETURN_VOID(env, jptsArray);
    AutoJavaFloatArray autoPts(env, jptsArray);
    float* floats = autoPts.ptr();
    const int length = autoPts.length();

    if ((offset | count) < 0 || offset + count > length) {
        doThrowAIOOBE(env);
        return;
    }

    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawPoints(floats + offset, count, *paint);
}

static void drawLine(JNIEnv* env, jobject, jlong canvasHandle, jfloat startX, jfloat startY,
                     jfloat stopX, jfloat stopY, jlong paintHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawLine(startX, startY, stopX, stopY, *paint);
}

static void drawLines(JNIEnv* env, jobject, jlong canvasHandle, jfloatArray jptsArray,
                      jint offset, jint count, jlong paintHandle) {
    NPE_CHECK_RETURN_VOID(env, jptsArray);
    AutoJavaFloatArray autoPts(env, jptsArray);
    float* floats = autoPts.ptr();
    const int length = autoPts.length();

    if ((offset | count) < 0 || offset + count > length) {
        doThrowAIOOBE(env);
        return;
    }

    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawLines(floats + offset, count, *paint);
}

static void drawRect(JNIEnv* env, jobject, jlong canvasHandle, jfloat left, jfloat top,
                     jfloat right, jfloat bottom, jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawRect(left, top, right, bottom, *paint);
}

static void drawRoundRect(JNIEnv* env, jobject, jlong canvasHandle, jfloat left, jfloat top,
                          jfloat right, jfloat bottom, jfloat rx, jfloat ry, jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawRoundRect(left, top, right, bottom, rx, ry, *paint);
}

static void drawCircle(JNIEnv* env, jobject, jlong canvasHandle, jfloat cx, jfloat cy,
                       jfloat radius, jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawCircle(cx, cy, radius, *paint);
}

static void drawOval(JNIEnv* env, jobject, jlong canvasHandle, jfloat left, jfloat top,
                     jfloat right, jfloat bottom, jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawOval(left, top, right, bottom, *paint);
}

static void drawArc(JNIEnv* env, jobject, jlong canvasHandle, jfloat left, jfloat top,
                    jfloat right, jfloat bottom, jfloat startAngle, jfloat sweepAngle,
                    jboolean useCenter, jlong paintHandle) {
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawArc(left, top, right, bottom, startAngle, sweepAngle,
                                       useCenter, *paint);
}

static void drawPath(JNIEnv* env, jobject, jlong canvasHandle, jlong pathHandle,
                     jlong paintHandle) {
    const SkPath* path = reinterpret_cast<SkPath*>(pathHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawPath(*path, *paint);
}

static void drawVertices(JNIEnv* env, jobject, jlong canvasHandle,
                         jint modeHandle, jint vertexCount,
                         jfloatArray jverts, jint vertIndex,
                         jfloatArray jtexs, jint texIndex,
                         jintArray jcolors, jint colorIndex,
                         jshortArray jindices, jint indexIndex,
                         jint indexCount, jlong paintHandle) {
    AutoJavaFloatArray  vertA(env, jverts, vertIndex + vertexCount);
    AutoJavaFloatArray  texA(env, jtexs, texIndex + vertexCount);
    AutoJavaIntArray    colorA(env, jcolors, colorIndex + vertexCount);
    AutoJavaShortArray  indexA(env, jindices, indexIndex + indexCount);

    const float* verts = vertA.ptr() + vertIndex;
    const float* texs = texA.ptr() + vertIndex;
    const int* colors = NULL;
    const uint16_t* indices = NULL;

    if (jcolors != NULL) {
        colors = colorA.ptr() + colorIndex;
    }
    if (jindices != NULL) {
        indices = (const uint16_t*)(indexA.ptr() + indexIndex);
    }

    SkCanvas::VertexMode mode = static_cast<SkCanvas::VertexMode>(modeHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawVertices(mode, vertexCount, verts, texs, colors,
                                           indices, indexCount, *paint);
}

static void drawBitmap(JNIEnv* env, jobject jcanvas, jlong canvasHandle, jlong bitmapHandle,
                       jfloat left, jfloat top, jlong paintHandle, jint canvasDensity,
                       jint screenDensity, jint bitmapDensity) {
    Canvas* canvas = get_canvas(canvasHandle);
    const SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);

    if (canvasDensity == bitmapDensity || canvasDensity == 0 || bitmapDensity == 0) {
        if (screenDensity != 0 && screenDensity != bitmapDensity) {
            SkPaint filteredPaint;
            if (paint) {
                filteredPaint = *paint;
            }
            filteredPaint.setFilterLevel(SkPaint::kLow_FilterLevel);
            canvas->drawBitmap(*bitmap, left, top, &filteredPaint);
        } else {
            canvas->drawBitmap(*bitmap, left, top, paint);
        }
    } else {
        canvas->save(SkCanvas::kMatrixClip_SaveFlag);
        SkScalar scale = canvasDensity / (float)bitmapDensity;
        canvas->translate(left, top);
        canvas->scale(scale, scale);

        SkPaint filteredPaint;
        if (paint) {
            filteredPaint = *paint;
        }
        filteredPaint.setFilterLevel(SkPaint::kLow_FilterLevel);

        canvas->drawBitmap(*bitmap, 0, 0, &filteredPaint);
        canvas->restore();
    }
}

static void drawBitmapMatrix(JNIEnv* env, jobject, jlong canvasHandle, jlong bitmapHandle,
                             jlong matrixHandle, jlong paintHandle) {
    const SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    const SkMatrix* matrix = reinterpret_cast<SkMatrix*>(matrixHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawBitmap(*bitmap, *matrix, paint);
}

static void drawBitmapRect(JNIEnv* env, jobject, jlong canvasHandle, jlong bitmapHandle,
                           float srcLeft, float srcTop, float srcRight, float srcBottom,
                           float dstLeft, float dstTop, float dstRight, float dstBottom,
                           jlong paintHandle, jint screenDensity, jint bitmapDensity) {
    Canvas* canvas = get_canvas(canvasHandle);
    const SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);

    if (screenDensity != 0 && screenDensity != bitmapDensity) {
        SkPaint filteredPaint;
        if (paint) {
            filteredPaint = *paint;
        }
        filteredPaint.setFilterLevel(SkPaint::kLow_FilterLevel);
        canvas->drawBitmap(*bitmap, srcLeft, srcTop, srcRight, srcBottom,
                           dstLeft, dstTop, dstRight, dstBottom, &filteredPaint);
    } else {
        canvas->drawBitmap(*bitmap, srcLeft, srcTop, srcRight, srcBottom,
                           dstLeft, dstTop, dstRight, dstBottom, paint);
    }
}

static void drawBitmapArray(JNIEnv* env, jobject, jlong canvasHandle,
                            jintArray jcolors, jint offset, jint stride,
                            jfloat x, jfloat y, jint width, jint height,
                            jboolean hasAlpha, jlong paintHandle) {
    // Note: If hasAlpha is false, kRGB_565_SkColorType will be used, which will
    // correct the alphaType to kOpaque_SkAlphaType.
    SkImageInfo info = SkImageInfo::Make(width, height,
                           hasAlpha ? kN32_SkColorType : kRGB_565_SkColorType,
                           kPremul_SkAlphaType);
    SkBitmap bitmap;
    if (!bitmap.allocPixels(info)) {
        return;
    }

    if (!GraphicsJNI::SetPixels(env, jcolors, offset, stride, 0, 0, width, height, bitmap)) {
        return;
    }

    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawBitmap(bitmap, x, y, paint);
}

static void drawBitmapMesh(JNIEnv* env, jobject, jlong canvasHandle, jlong bitmapHandle,
                           jint meshWidth, jint meshHeight, jfloatArray jverts,
                           jint vertIndex, jintArray jcolors, jint colorIndex, jlong paintHandle) {
    const int ptCount = (meshWidth + 1) * (meshHeight + 1);
    AutoJavaFloatArray vertA(env, jverts, vertIndex + (ptCount << 1));
    AutoJavaIntArray colorA(env, jcolors, colorIndex + ptCount);

    const SkBitmap* bitmap = reinterpret_cast<SkBitmap*>(bitmapHandle);
    const SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    get_canvas(canvasHandle)->drawBitmapMesh(*bitmap, meshWidth, meshHeight,
                                             vertA.ptr(), colorA.ptr(), paint);
}

static void drawTextChars(JNIEnv* env, jobject, jlong canvasHandle, jcharArray text,
                          jint index, jint count, jfloat x, jfloat y, jint bidiFlags,
                          jlong paintHandle, jlong typefaceHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);
    jchar* jchars = env->GetCharArrayElements(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars) + index;
    get_canvas(canvasHandle)->drawText(textArray, 0, count, count, x, y, bidiFlags, *paint, typeface);
    env->ReleaseCharArrayElements(text, jchars, JNI_ABORT);
}

static void drawTextString(JNIEnv* env, jobject, jlong canvasHandle, jstring text,
                           jint start, jint end, jfloat x, jfloat y, jint bidiFlags,
                           jlong paintHandle, jlong typefaceHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);
    const int count = end - start;
    const jchar* jchars = env->GetStringChars(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars) + start;
    get_canvas(canvasHandle)->drawText(textArray, 0, count, count, x, y, bidiFlags, *paint, typeface);
    env->ReleaseStringChars(text, jchars);
}

static void drawTextRunChars(JNIEnv* env, jobject, jlong canvasHandle, jcharArray text, jint index,
                             jint count, jint contextIndex, jint contextCount, jfloat x, jfloat y,
                             jboolean isRtl, jlong paintHandle, jlong typefaceHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);

    const int bidiFlags = isRtl ? kBidi_Force_RTL : kBidi_Force_LTR;
    jchar* jchars = env->GetCharArrayElements(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars) + contextIndex;
    get_canvas(canvasHandle)->drawText(textArray, index - contextIndex, count, contextCount,
                                       x, y, bidiFlags, *paint, typeface);
    env->ReleaseCharArrayElements(text, jchars, JNI_ABORT);
}

static void drawTextRunString(JNIEnv* env, jobject obj, jlong canvasHandle, jstring text,
                              jint start, jint end, jint contextStart, jint contextEnd,
                              jfloat x, jfloat y, jboolean isRtl, jlong paintHandle,
                              jlong typefaceHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);

    int bidiFlags = isRtl ? kBidi_Force_RTL : kBidi_Force_LTR;
    jint count = end - start;
    jint contextCount = contextEnd - contextStart;
    const jchar* jchars = env->GetStringChars(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars) + contextStart;
    get_canvas(canvasHandle)->drawText(textArray, start - contextStart, count, contextCount,
                                       x, y, bidiFlags, *paint, typeface);
    env->ReleaseStringChars(text, jchars);
}

static void drawPosTextChars(JNIEnv* env, jobject, jlong canvasHandle, jcharArray text,
                             jint index, jint count, jfloatArray pos, jlong paintHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    jchar* jchars = text ? env->GetCharArrayElements(text, NULL) : NULL;
    float* posArray = pos ? env->GetFloatArrayElements(pos, NULL) : NULL;
    int posCount = pos ? env->GetArrayLength(pos) >> 1: 0;

    const char* textArray = reinterpret_cast<const char*>(jchars) + index;
    get_canvas(canvasHandle)->drawPosText(textArray, posArray, count << 1, posCount, *paint);

    if (text) {
        env->ReleaseCharArrayElements(text, jchars, 0);
    }
    if (pos) {
        env->ReleaseFloatArrayElements(pos, posArray, 0);
    }
}


static void drawPosTextString(JNIEnv* env, jobject, jlong canvasHandle, jstring text,
                              jfloatArray pos, jlong paintHandle) {
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    const jchar* jchars = text ? env->GetStringChars(text, NULL) : NULL;
    int byteLength = text ? env->GetStringLength(text) : 0;
    float* posArray = pos ? env->GetFloatArrayElements(pos, NULL) : NULL;
    int posCount = pos ? env->GetArrayLength(pos) >> 1: 0;

    const char* textArray = reinterpret_cast<const char*>(jchars);
    get_canvas(canvasHandle)->drawPosText(textArray , posArray, byteLength << 1, posCount, *paint);

    if (text) {
        env->ReleaseStringChars(text, jchars);
    }
    if (pos) {
        env->ReleaseFloatArrayElements(pos, posArray, 0);
    }
}

class DrawTextOnPathFunctor {
public:
    DrawTextOnPathFunctor(const Layout& layout, Canvas* canvas, float hOffset,
                float vOffset, const SkPaint& paint, const SkPath& path)
            : layout(layout), canvas(canvas), hOffset(hOffset), vOffset(vOffset),
                paint(paint), path(path) {
    }
    void operator()(size_t start, size_t end) {
        uint16_t glyphs[1];
        for (size_t i = start; i < end; i++) {
            glyphs[0] = layout.getGlyphId(i);
            float x = hOffset + layout.getX(i);
            float y = vOffset + layout.getY(i);
            canvas->drawTextOnPath((const char*) glyphs, 1, path, x, y, paint);
        }
    }
private:
    const Layout& layout;
    Canvas* canvas;
    float hOffset;
    float vOffset;
    const SkPaint& paint;
    const SkPath& path;
};

static void drawTextOnPath(Canvas* canvas, const char* text, int count, int bidiFlags,
                           const SkPath& path, float hOffset, float vOffset,
                           const SkPaint& paint, TypefaceImpl* typeface) {
    SkPaint paintCopy(paint);
    Layout layout;
    std::string css = MinikinUtils::setLayoutProperties(&layout, &paintCopy, bidiFlags, typeface);
    layout.doLayout((uint16_t*)text, 0, count, count, css);
    hOffset += MinikinUtils::hOffsetForTextAlign(&paintCopy, layout, path);

    // Set align to left for drawing, as we don't want individual
    // glyphs centered or right-aligned; the offset above takes
    // care of all alignment.
    paintCopy.setTextAlign(SkPaint::kLeft_Align);

    DrawTextOnPathFunctor f(layout, canvas, hOffset, vOffset, paintCopy, path);
    MinikinUtils::forFontRun(layout, &paintCopy, f);
}

static void drawTextOnPathChars(JNIEnv* env, jobject, jlong canvasHandle, jcharArray text,
                                jint index, jint count, jlong pathHandle, jfloat hOffset,
                                jfloat vOffset, jint bidiFlags, jlong paintHandle,
                                jlong typefaceHandle) {
    SkPath* path = reinterpret_cast<SkPath*>(pathHandle);
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);

    jchar* jchars = env->GetCharArrayElements(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars);

    drawTextOnPath(get_canvas(canvasHandle), textArray + index, count, bidiFlags, *path,
                   hOffset, vOffset, *paint, typeface);

    env->ReleaseCharArrayElements(text, jchars, 0);
}

static void drawTextOnPathString(JNIEnv* env, jobject, jlong canvasHandle, jstring text,
                                 jlong pathHandle, jfloat hOffset, jfloat vOffset,
                                 jint bidiFlags, jlong paintHandle, jlong typefaceHandle) {
    SkPath* path = reinterpret_cast<SkPath*>(pathHandle);
    SkPaint* paint = reinterpret_cast<SkPaint*>(paintHandle);
    TypefaceImpl* typeface = reinterpret_cast<TypefaceImpl*>(typefaceHandle);

    const jchar* jchars = env->GetStringChars(text, NULL);
    const char* textArray = reinterpret_cast<const char*>(jchars);
    int count = env->GetStringLength(text);

    drawTextOnPath(get_canvas(canvasHandle), textArray, count, bidiFlags, *path,
                   hOffset, vOffset, *paint, typeface);

    env->ReleaseStringChars(text, jchars);
}

static void setDrawFilter(JNIEnv* env, jobject, jlong canvasHandle, jlong filterHandle) {
    get_canvas(canvasHandle)->setDrawFilter(reinterpret_cast<SkDrawFilter*>(filterHandle));
}

static void freeCaches(JNIEnv* env, jobject) {
    SkGraphics::PurgeFontCache();
}

static void freeTextLayoutCaches(JNIEnv* env, jobject) {
    Layout::purgeCaches();
}

}; // namespace CanvasJNI

static JNINativeMethod gMethods[] = {
    {"finalizer", "(J)V", (void*) CanvasJNI::finalizer},
    {"initRaster", "(J)J", (void*) CanvasJNI::initRaster},
    {"native_setBitmap", "(JJZ)V", (void*) CanvasJNI::setBitmap},
    {"native_isOpaque","(J)Z", (void*) CanvasJNI::isOpaque},
    {"native_getWidth","(J)I", (void*) CanvasJNI::getWidth},
    {"native_getHeight","(J)I", (void*) CanvasJNI::getHeight},
    {"native_save","(JI)I", (void*) CanvasJNI::save},
    {"native_saveLayer","(JFFFFJI)I", (void*) CanvasJNI::saveLayer},
    {"native_saveLayerAlpha","(JFFFFII)I", (void*) CanvasJNI::saveLayerAlpha},
    {"native_getSaveCount","(J)I", (void*) CanvasJNI::getSaveCount},
    {"native_restore","(J)V", (void*) CanvasJNI::restore},
    {"native_restoreToCount","(JI)V", (void*) CanvasJNI::restoreToCount},
    {"native_getCTM", "(JJ)V", (void*)CanvasJNI::getCTM},
    {"native_setMatrix","(JJ)V", (void*) CanvasJNI::setMatrix},
    {"native_concat","(JJ)V", (void*) CanvasJNI::concat},
    {"native_rotate","(JF)V", (void*) CanvasJNI::rotate},
    {"native_scale","(JFF)V", (void*) CanvasJNI::scale},
    {"native_skew","(JFF)V", (void*) CanvasJNI::skew},
    {"native_translate","(JFF)V", (void*) CanvasJNI::translate},
    {"native_getClipBounds","(JLandroid/graphics/Rect;)Z", (void*) CanvasJNI::getClipBounds},
    {"native_quickReject","(JJ)Z", (void*) CanvasJNI::quickRejectPath},
    {"native_quickReject","(JFFFF)Z", (void*)CanvasJNI::quickRejectRect},
    {"native_clipRect","(JFFFFI)Z", (void*) CanvasJNI::clipRect},
    {"native_clipPath","(JJI)Z", (void*) CanvasJNI::clipPath},
    {"native_clipRegion","(JJI)Z", (void*) CanvasJNI::clipRegion},
    {"native_drawColor","(JII)V", (void*) CanvasJNI::drawColor},
    {"native_drawPaint","(JJ)V", (void*) CanvasJNI::drawPaint},
    {"native_drawPoint", "(JFFJ)V", (void*) CanvasJNI::drawPoint},
    {"native_drawPoints", "(J[FIIJ)V", (void*) CanvasJNI::drawPoints},
    {"native_drawLine", "(JFFFFJ)V", (void*) CanvasJNI::drawLine},
    {"native_drawLines", "(J[FIIJ)V", (void*) CanvasJNI::drawLines},
    {"native_drawRect","(JFFFFJ)V", (void*) CanvasJNI::drawRect},
    {"native_drawRoundRect","(JFFFFFFJ)V", (void*) CanvasJNI::drawRoundRect},
    {"native_drawCircle","(JFFFJ)V", (void*) CanvasJNI::drawCircle},
    {"native_drawOval","(JFFFFJ)V", (void*) CanvasJNI::drawOval},
    {"native_drawArc","(JFFFFFFZJ)V", (void*) CanvasJNI::drawArc},
    {"native_drawPath","(JJJ)V", (void*) CanvasJNI::drawPath},
    {"nativeDrawVertices", "(JII[FI[FI[II[SIIJ)V", (void*)CanvasJNI::drawVertices},
    {"native_drawBitmap","(JJFFJIII)V", (void*) CanvasJNI::drawBitmap},
    {"nativeDrawBitmapMatrix", "(JJJJ)V", (void*)CanvasJNI::drawBitmapMatrix},
    {"native_drawBitmap","(JJFFFFFFFFJII)V", (void*) CanvasJNI::drawBitmapRect},
    {"native_drawBitmap", "(J[IIIFFIIZJ)V", (void*)CanvasJNI::drawBitmapArray},
    {"nativeDrawBitmapMesh", "(JJII[FI[IIJ)V", (void*)CanvasJNI::drawBitmapMesh},
    {"native_drawText","(J[CIIFFIJJ)V", (void*) CanvasJNI::drawTextChars},
    {"native_drawText","(JLjava/lang/String;IIFFIJJ)V", (void*) CanvasJNI::drawTextString},
    {"native_drawTextRun","(J[CIIIIFFZJJ)V", (void*) CanvasJNI::drawTextRunChars},
    {"native_drawTextRun","(JLjava/lang/String;IIIIFFZJJ)V", (void*) CanvasJNI::drawTextRunString},
    {"native_drawTextOnPath","(J[CIIJFFIJJ)V", (void*) CanvasJNI::drawTextOnPathChars},
    {"native_drawTextOnPath","(JLjava/lang/String;JFFIJJ)V", (void*) CanvasJNI::drawTextOnPathString},
    {"nativeSetDrawFilter", "(JJ)V", (void*) CanvasJNI::setDrawFilter},
    {"freeCaches", "()V", (void*) CanvasJNI::freeCaches},
    {"freeTextLayoutCaches", "()V", (void*) CanvasJNI::freeTextLayoutCaches}
};

int register_android_graphics_Canvas(JNIEnv* env) {
    return AndroidRuntime::registerNativeMethods(env, "android/graphics/Canvas", gMethods, NELEM(gMethods));
}

}; // namespace android