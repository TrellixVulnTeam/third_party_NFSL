/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkRect.h"
#include "Test.h"

static const char* boolStr(bool value) {
    return value ? "true" : "false";
}

// these are in the same order as the SkColorType enum
static const char* gColorTypeName[] = {
    "None", "A8", "565", "4444", "RGBA", "BGRA", "Index8"
};

static void report_opaqueness(skiatest::Reporter* reporter, const SkBitmap& src,
                              const SkBitmap& dst) {
    ERRORF(reporter, "src %s opaque:%d, dst %s opaque:%d",
           gColorTypeName[src.colorType()], src.isOpaque(),
           gColorTypeName[dst.colorType()], dst.isOpaque());
}

static bool canHaveAlpha(SkColorType ct) {
    return kRGB_565_SkColorType != ct;
}

// copyTo() should preserve isOpaque when it makes sense
static void test_isOpaque(skiatest::Reporter* reporter,
                          const SkBitmap& srcOpaque, const SkBitmap& srcPremul,
                          SkColorType dstColorType) {
    SkBitmap dst;

    if (canHaveAlpha(srcPremul.colorType()) && canHaveAlpha(dstColorType)) {
        REPORTER_ASSERT(reporter, srcPremul.copyTo(&dst, dstColorType));
        REPORTER_ASSERT(reporter, dst.colorType() == dstColorType);
        if (srcPremul.isOpaque() != dst.isOpaque()) {
            report_opaqueness(reporter, srcPremul, dst);
        }
    }

    REPORTER_ASSERT(reporter, srcOpaque.copyTo(&dst, dstColorType));
    REPORTER_ASSERT(reporter, dst.colorType() == dstColorType);
    if (srcOpaque.isOpaque() != dst.isOpaque()) {
        report_opaqueness(reporter, srcOpaque, dst);
    }
}

static void init_src(const SkBitmap& bitmap) {
    SkAutoLockPixels lock(bitmap);
    if (bitmap.getPixels()) {
        if (bitmap.getColorTable()) {
            sk_bzero(bitmap.getPixels(), bitmap.getSize());
        } else {
            bitmap.eraseColor(SK_ColorWHITE);
        }
    }
}

static SkColorTable* init_ctable(SkAlphaType alphaType) {
    static const SkColor colors[] = {
        SK_ColorBLACK, SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE
    };
    return new SkColorTable(colors, SK_ARRAY_COUNT(colors), alphaType);
}

struct Pair {
    SkColorType fColorType;
    const char* fValid;
};

// Utility functions for copyPixelsTo()/copyPixelsFrom() tests.
// getPixel()
// setPixel()
// getSkConfigName()
// struct Coordinates
// reportCopyVerification()
// writeCoordPixels()

// Utility function to read the value of a given pixel in bm. All
// values converted to uint32_t for simplification of comparisons.
static uint32_t getPixel(int x, int y, const SkBitmap& bm) {
    uint32_t val = 0;
    uint16_t val16;
    uint8_t val8;
    SkAutoLockPixels lock(bm);
    const void* rawAddr = bm.getAddr(x,y);

    switch (bm.bytesPerPixel()) {
        case 4:
            memcpy(&val, rawAddr, sizeof(uint32_t));
            break;
        case 2:
            memcpy(&val16, rawAddr, sizeof(uint16_t));
            val = val16;
            break;
        case 1:
            memcpy(&val8, rawAddr, sizeof(uint8_t));
            val = val8;
            break;
        default:
            break;
    }
    return val;
}

// Utility function to set value of any pixel in bm.
// bm.getConfig() specifies what format 'val' must be
// converted to, but at present uint32_t can handle all formats.
static void setPixel(int x, int y, uint32_t val, SkBitmap& bm) {
    uint16_t val16;
    uint8_t val8;
    SkAutoLockPixels lock(bm);
    void* rawAddr = bm.getAddr(x,y);

    switch (bm.bytesPerPixel()) {
        case 4:
            memcpy(rawAddr, &val, sizeof(uint32_t));
            break;
        case 2:
            val16 = val & 0xFFFF;
            memcpy(rawAddr, &val16, sizeof(uint16_t));
            break;
        case 1:
            val8 = val & 0xFF;
            memcpy(rawAddr, &val8, sizeof(uint8_t));
            break;
        default:
            // Ignore.
            break;
    }
}

// Helper struct to contain pixel locations, while avoiding need for STL.
struct Coordinates {

    const int length;
    SkIPoint* const data;

    explicit Coordinates(int _length): length(_length)
                                     , data(new SkIPoint[length]) { }

    ~Coordinates(){
        delete [] data;
    }

    SkIPoint* operator[](int i) const {
        // Use with care, no bounds checking.
        return data + i;
    }
};

// A function to verify that two bitmaps contain the same pixel values
// at all coordinates indicated by coords. Simplifies verification of
// copied bitmaps.
static void reportCopyVerification(const SkBitmap& bm1, const SkBitmap& bm2,
                            Coordinates& coords,
                            const char* msg,
                            skiatest::Reporter* reporter){
    bool success = true;

    // Confirm all pixels in the list match.
    for (int i = 0; i < coords.length; ++i) {
        success = success &&
                  (getPixel(coords[i]->fX, coords[i]->fY, bm1) ==
                   getPixel(coords[i]->fX, coords[i]->fY, bm2));
    }

    if (!success) {
        ERRORF(reporter, "%s [colortype = %s]", msg,
               gColorTypeName[bm1.colorType()]);
    }
}

// Writes unique pixel values at locations specified by coords.
static void writeCoordPixels(SkBitmap& bm, const Coordinates& coords) {
    for (int i = 0; i < coords.length; ++i)
        setPixel(coords[i]->fX, coords[i]->fY, i, bm);
}

static const Pair gPairs[] = {
    { kUnknown_SkColorType,     "000000"  },
    { kAlpha_8_SkColorType,     "010101"  },
    { kIndex_8_SkColorType,     "011101"  },
    { kRGB_565_SkColorType,     "010101"  },
    { kARGB_4444_SkColorType,   "010111"  },
    { kPMColor_SkColorType,     "010111"  },
};

static const int W = 20;
static const int H = 33;

static void setup_src_bitmaps(SkBitmap* srcOpaque, SkBitmap* srcPremul,
                              SkColorType ct) {
    SkColorTable* ctOpaque = NULL;
    SkColorTable* ctPremul = NULL;
    if (kIndex_8_SkColorType == ct) {
        ctOpaque = init_ctable(kOpaque_SkAlphaType);
        ctPremul = init_ctable(kPremul_SkAlphaType);
    }

    srcOpaque->allocPixels(SkImageInfo::Make(W, H, ct, kOpaque_SkAlphaType),
                           NULL, ctOpaque);
    srcPremul->allocPixels(SkImageInfo::Make(W, H, ct, kPremul_SkAlphaType),
                           NULL, ctPremul);
    SkSafeUnref(ctOpaque);
    SkSafeUnref(ctPremul);
    init_src(*srcOpaque);
    init_src(*srcPremul);
}

DEF_TEST(BitmapCopy_extractSubset, reporter) {
    for (size_t i = 0; i < SK_ARRAY_COUNT(gPairs); i++) {
        SkBitmap srcOpaque, srcPremul;
        setup_src_bitmaps(&srcOpaque, &srcPremul, gPairs[i].fColorType);

        SkBitmap bitmap(srcOpaque);
        SkBitmap subset;
        SkIRect r;
        // Extract a subset which has the same width as the original. This
        // catches a bug where we cloned the genID incorrectly.
        r.set(0, 1, W, 3);
        bitmap.setIsVolatile(true);
        // Relies on old behavior of extractSubset failing if colortype is unknown
        if (kUnknown_SkColorType != bitmap.colorType() && bitmap.extractSubset(&subset, r)) {
            REPORTER_ASSERT(reporter, subset.width() == W);
            REPORTER_ASSERT(reporter, subset.height() == 2);
            REPORTER_ASSERT(reporter, subset.alphaType() == bitmap.alphaType());
            REPORTER_ASSERT(reporter, subset.isVolatile() == true);

            // Test copying an extracted subset.
            for (size_t j = 0; j < SK_ARRAY_COUNT(gPairs); j++) {
                SkBitmap copy;
                bool success = subset.copyTo(&copy, gPairs[j].fColorType);
                if (!success) {
                    // Skip checking that success matches fValid, which is redundant
                    // with the code below.
                    REPORTER_ASSERT(reporter, gPairs[i].fColorType != gPairs[j].fColorType);
                    continue;
                }

                // When performing a copy of an extracted subset, the gen id should
                // change.
                REPORTER_ASSERT(reporter, copy.getGenerationID() != subset.getGenerationID());

                REPORTER_ASSERT(reporter, copy.width() == W);
                REPORTER_ASSERT(reporter, copy.height() == 2);

                if (gPairs[i].fColorType == gPairs[j].fColorType) {
                    SkAutoLockPixels alp0(subset);
                    SkAutoLockPixels alp1(copy);
                    // they should both have, or both not-have, a colortable
                    bool hasCT = subset.getColorTable() != NULL;
                    REPORTER_ASSERT(reporter, (copy.getColorTable() != NULL) == hasCT);
                }
            }
        }

        bitmap = srcPremul;
        bitmap.setIsVolatile(false);
        if (bitmap.extractSubset(&subset, r)) {
            REPORTER_ASSERT(reporter, subset.alphaType() == bitmap.alphaType());
            REPORTER_ASSERT(reporter, subset.isVolatile() == false);
        }
    }
}

DEF_TEST(BitmapCopy, reporter) {
    static const bool isExtracted[] = {
        false, true
    };

    for (size_t i = 0; i < SK_ARRAY_COUNT(gPairs); i++) {
        SkBitmap srcOpaque, srcPremul;
        setup_src_bitmaps(&srcOpaque, &srcPremul, gPairs[i].fColorType);

        for (size_t j = 0; j < SK_ARRAY_COUNT(gPairs); j++) {
            SkBitmap dst;

            bool success = srcPremul.copyTo(&dst, gPairs[j].fColorType);
            bool expected = gPairs[i].fValid[j] != '0';
            if (success != expected) {
                ERRORF(reporter, "SkBitmap::copyTo from %s to %s. expected %s "
                       "returned %s", gColorTypeName[i], gColorTypeName[j],
                       boolStr(expected), boolStr(success));
            }

            bool canSucceed = srcPremul.canCopyTo(gPairs[j].fColorType);
            if (success != canSucceed) {
                ERRORF(reporter, "SkBitmap::copyTo from %s to %s. returned %s "
                       "canCopyTo %s", gColorTypeName[i], gColorTypeName[j],
                       boolStr(success), boolStr(canSucceed));
            }

            if (success) {
                REPORTER_ASSERT(reporter, srcPremul.width() == dst.width());
                REPORTER_ASSERT(reporter, srcPremul.height() == dst.height());
                REPORTER_ASSERT(reporter, dst.colorType() == gPairs[j].fColorType);
                test_isOpaque(reporter, srcOpaque, srcPremul, dst.colorType());
                if (srcPremul.colorType() == dst.colorType()) {
                    SkAutoLockPixels srcLock(srcPremul);
                    SkAutoLockPixels dstLock(dst);
                    REPORTER_ASSERT(reporter, srcPremul.readyToDraw());
                    REPORTER_ASSERT(reporter, dst.readyToDraw());
                    const char* srcP = (const char*)srcPremul.getAddr(0, 0);
                    const char* dstP = (const char*)dst.getAddr(0, 0);
                    REPORTER_ASSERT(reporter, srcP != dstP);
                    REPORTER_ASSERT(reporter, !memcmp(srcP, dstP,
                                                      srcPremul.getSize()));
                    REPORTER_ASSERT(reporter, srcPremul.getGenerationID() == dst.getGenerationID());
                } else {
                    REPORTER_ASSERT(reporter, srcPremul.getGenerationID() != dst.getGenerationID());
                }
            } else {
                // dst should be unchanged from its initial state
                REPORTER_ASSERT(reporter, dst.colorType() == kUnknown_SkColorType);
                REPORTER_ASSERT(reporter, dst.width() == 0);
                REPORTER_ASSERT(reporter, dst.height() == 0);
            }
        } // for (size_t j = ...

        // Tests for getSafeSize(), getSafeSize64(), copyPixelsTo(),
        // copyPixelsFrom().
        //
        for (size_t copyCase = 0; copyCase < SK_ARRAY_COUNT(isExtracted);
             ++copyCase) {
            // Test copying to/from external buffer.
            // Note: the tests below have hard-coded values ---
            //       Please take care if modifying.

            // Tests for getSafeSize64().
            // Test with a very large configuration without pixel buffer
            // attached.
            SkBitmap tstSafeSize;
            tstSafeSize.setConfig(SkImageInfo::Make(100000000U, 100000000U,
                                                    gPairs[i].fColorType,
                                                    kPremul_SkAlphaType));
            int64_t safeSize = tstSafeSize.computeSafeSize64();
            if (safeSize < 0) {
                ERRORF(reporter, "getSafeSize64() negative: %s",
                       gColorTypeName[tstSafeSize.colorType()]);
            }
            bool sizeFail = false;
            // Compare against hand-computed values.
            switch (gPairs[i].fColorType) {
                case kUnknown_SkColorType:
                    break;

                case kAlpha_8_SkColorType:
                case kIndex_8_SkColorType:
                    if (safeSize != 0x2386F26FC10000LL) {
                        sizeFail = true;
                    }
                    break;

                case kRGB_565_SkColorType:
                case kARGB_4444_SkColorType:
                    if (safeSize != 0x470DE4DF820000LL) {
                        sizeFail = true;
                    }
                    break;

                case kPMColor_SkColorType:
                    if (safeSize != 0x8E1BC9BF040000LL) {
                        sizeFail = true;
                    }
                    break;

                default:
                    break;
            }
            if (sizeFail) {
                ERRORF(reporter, "computeSafeSize64() wrong size: %s",
                       gColorTypeName[tstSafeSize.colorType()]);
            }

            int subW = 2;
            int subH = 2;

            // Create bitmap to act as source for copies and subsets.
            SkBitmap src, subset;
            SkColorTable* ct = NULL;
            if (kIndex_8_SkColorType == src.colorType()) {
                ct = init_ctable(kPremul_SkAlphaType);
            }

            if (isExtracted[copyCase]) { // A larger image to extract from.
                src.allocPixels(SkImageInfo::Make(2 * subW + 1, subH,
                                                  gPairs[i].fColorType,
                                                  kPremul_SkAlphaType));
            } else { // Tests expect a 2x2 bitmap, so make smaller.
                src.allocPixels(SkImageInfo::Make(subW, subH,
                                                  gPairs[i].fColorType,
                                                  kPremul_SkAlphaType));
            }
            SkSafeUnref(ct);

            // Either copy src or extract into 'subset', which is used
            // for subsequent calls to copyPixelsTo/From.
            bool srcReady = false;
            // Test relies on older behavior that extractSubset will fail on
            // kUnknown_SkColorType
            if (kUnknown_SkColorType != src.colorType() &&
                isExtracted[copyCase]) {
                // The extractedSubset() test case allows us to test copy-
                // ing when src and dst mave possibly different strides.
                SkIRect r;
                r.set(1, 0, 1 + subW, subH); // 2x2 extracted bitmap

                srcReady = src.extractSubset(&subset, r);
            } else {
                srcReady = src.copyTo(&subset);
            }

            // Not all configurations will generate a valid 'subset'.
            if (srcReady) {

                // Allocate our target buffer 'buf' for all copies.
                // To simplify verifying correctness of copies attach
                // buf to a SkBitmap, but copies are done using the
                // raw buffer pointer.
                const size_t bufSize = subH *
                    SkColorTypeMinRowBytes(src.colorType(), subW) * 2;
                SkAutoMalloc autoBuf (bufSize);
                uint8_t* buf = static_cast<uint8_t*>(autoBuf.get());

                SkBitmap bufBm; // Attach buf to this bitmap.
                bool successExpected;

                // Set up values for each pixel being copied.
                Coordinates coords(subW * subH);
                for (int x = 0; x < subW; ++x)
                    for (int y = 0; y < subH; ++y)
                    {
                        int index = y * subW + x;
                        SkASSERT(index < coords.length);
                        coords[index]->fX = x;
                        coords[index]->fY = y;
                    }

                writeCoordPixels(subset, coords);

                // Test #1 ////////////////////////////////////////////

                const SkImageInfo info = SkImageInfo::Make(subW, subH,
                                                           gPairs[i].fColorType,
                                                           kPremul_SkAlphaType);
                // Before/after comparisons easier if we attach buf
                // to an appropriately configured SkBitmap.
                memset(buf, 0xFF, bufSize);
                // Config with stride greater than src but that fits in buf.
                bufBm.installPixels(info, buf, info.minRowBytes() * 2);
                successExpected = false;
                // Then attempt to copy with a stride that is too large
                // to fit in the buffer.
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsTo(buf, bufSize, bufBm.rowBytes() * 3)
                    == successExpected);

                if (successExpected)
                    reportCopyVerification(subset, bufBm, coords,
                        "copyPixelsTo(buf, bufSize, 1.5*maxRowBytes)",
                        reporter);

                // Test #2 ////////////////////////////////////////////
                // This test should always succeed, but in the case
                // of extracted bitmaps only because we handle the
                // issue of getSafeSize(). Without getSafeSize()
                // buffer overrun/read would occur.
                memset(buf, 0xFF, bufSize);
                bufBm.installPixels(info, buf, subset.rowBytes());
                successExpected = subset.getSafeSize() <= bufSize;
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsTo(buf, bufSize) ==
                        successExpected);
                if (successExpected)
                    reportCopyVerification(subset, bufBm, coords,
                    "copyPixelsTo(buf, bufSize)", reporter);

                // Test #3 ////////////////////////////////////////////
                // Copy with different stride between src and dst.
                memset(buf, 0xFF, bufSize);
                bufBm.installPixels(info, buf, subset.rowBytes()+1);
                successExpected = true; // Should always work.
                REPORTER_ASSERT(reporter,
                        subset.copyPixelsTo(buf, bufSize,
                            subset.rowBytes()+1) == successExpected);
                if (successExpected)
                    reportCopyVerification(subset, bufBm, coords,
                    "copyPixelsTo(buf, bufSize, rowBytes+1)", reporter);

                // Test #4 ////////////////////////////////////////////
                // Test copy with stride too small.
                memset(buf, 0xFF, bufSize);
                bufBm.installPixels(info, buf, info.minRowBytes());
                successExpected = false;
                // Request copy with stride too small.
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsTo(buf, bufSize, bufBm.rowBytes()-1)
                        == successExpected);
                if (successExpected)
                    reportCopyVerification(subset, bufBm, coords,
                    "copyPixelsTo(buf, bufSize, rowBytes()-1)", reporter);

#if 0   // copyPixelsFrom is gone
                // Test #5 ////////////////////////////////////////////
                // Tests the case where the source stride is too small
                // for the source configuration.
                memset(buf, 0xFF, bufSize);
                bufBm.installPixels(info, buf, info.minRowBytes());
                writeCoordPixels(bufBm, coords);
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsFrom(buf, bufSize, 1) == false);

                // Test #6 ///////////////////////////////////////////
                // Tests basic copy from an external buffer to the bitmap.
                // If the bitmap is "extracted", this also tests the case
                // where the source stride is different from the dest.
                // stride.
                // We've made the buffer large enough to always succeed.
                bufBm.installPixels(info, buf, info.minRowBytes());
                writeCoordPixels(bufBm, coords);
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsFrom(buf, bufSize, bufBm.rowBytes()) ==
                        true);
                reportCopyVerification(bufBm, subset, coords,
                    "copyPixelsFrom(buf, bufSize)",
                    reporter);

                // Test #7 ////////////////////////////////////////////
                // Tests the case where the source buffer is too small
                // for the transfer.
                REPORTER_ASSERT(reporter,
                    subset.copyPixelsFrom(buf, 1, subset.rowBytes()) ==
                        false);

#endif
            }
        } // for (size_t copyCase ...
    }
}