/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>

#include <string_view>

#include <test/bootstrapfixture.hxx>
#include <unotest/macros_test.hxx>
#include <test/xmltesttools.hxx>

#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>

#include <comphelper/propertysequence.hxx>
#include <unotools/tempfile.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <rtl/character.hxx>

using namespace ::com::sun::star;

constexpr OUStringLiteral DATA_DIRECTORY = u"/xmloff/qa/unit/data/";

/// Covers xmloff/source/style/ fixes.
class XmloffStyleTest : public test::BootstrapFixture,
                        public unotest::MacrosTest,
                        public XmlTestTools
{
private:
    uno::Reference<lang::XComponent> mxComponent;

public:
    void setUp() override;
    void tearDown() override;
    void registerNamespaces(xmlXPathContextPtr& pXmlXpathCtx) override;
    uno::Reference<lang::XComponent>& getComponent() { return mxComponent; }
    void load(std::u16string_view rURL);
};

void XmloffStyleTest::setUp()
{
    test::BootstrapFixture::setUp();

    mxDesktop.set(frame::Desktop::create(mxComponentContext));
}

void XmloffStyleTest::tearDown()
{
    if (mxComponent.is())
        mxComponent->dispose();

    test::BootstrapFixture::tearDown();
}

void XmloffStyleTest::registerNamespaces(xmlXPathContextPtr& pXmlXpathCtx)
{
    XmlTestTools::registerODFNamespaces(pXmlXpathCtx);
}

void XmloffStyleTest::load(std::u16string_view rFileName)
{
    OUString aURL = m_directories.getURLFromSrc(DATA_DIRECTORY) + rFileName;
    mxComponent = loadFromDesktop(aURL);
}

CPPUNIT_TEST_FIXTURE(XmloffStyleTest, testFillImageBase64)
{
    // Load a flat ODG that has base64-encoded bitmap as a fill style.
    load(u"fill-image-base64.fodg");
    uno::Reference<lang::XMultiServiceFactory> xFactory(getComponent(), uno::UNO_QUERY);
    uno::Reference<container::XNameContainer> xBitmaps(
        xFactory->createInstance("com.sun.star.drawing.BitmapTable"), uno::UNO_QUERY);

    // Without the accompanying fix in place, this test would have failed, as the base64 stream was
    // not considered when parsing the fill-image style.
    CPPUNIT_ASSERT(xBitmaps->hasByName("libreoffice_0"));
}

namespace
{
struct XmlFont
{
    OString aName;
    OString aFontFamilyGeneric;
    bool operator<(const XmlFont& rOther) const
    {
        sal_Int32 nRet = aName.compareTo(rOther.aName);
        if (nRet != 0)
        {
            return nRet < 0;
        }

        return aFontFamilyGeneric.compareTo(rOther.aFontFamilyGeneric) < 0;
    }
};
}

CPPUNIT_TEST_FIXTURE(XmloffStyleTest, testFontSorting)
{
    // Given an empty document with default fonts (Liberation Sans, Lucida Sans, etc):
    getComponent() = loadFromDesktop("private:factory/swriter");

    // When saving that document to ODT:
    uno::Reference<frame::XStorable> xStorable(getComponent(), uno::UNO_QUERY);
    utl::TempFile aTempFile;
    aTempFile.EnableKillingFile();
    uno::Sequence<beans::PropertyValue> aStoreProps = comphelper::InitPropertySequence({
        { "FilterName", uno::makeAny(OUString("writer8")) },
    });
    xStorable->storeToURL(aTempFile.GetURL(), aStoreProps);

    // Then make sure <style:font-face> elements are sorted (by style:name="..."):
    uno::Reference<packages::zip::XZipFileAccess2> xNameAccess
        = packages::zip::ZipFileAccess::createWithURL(mxComponentContext, aTempFile.GetURL());
    uno::Reference<io::XInputStream> xInputStream(xNameAccess->getByName("content.xml"),
                                                  uno::UNO_QUERY);
    std::unique_ptr<SvStream> pStream(utl::UcbStreamHelper::CreateStream(xInputStream, true));
    xmlDocUniquePtr pXmlDoc = parseXmlStream(pStream.get());
    xmlXPathObjectPtr pXPath
        = getXPathNode(pXmlDoc, "/office:document-content/office:font-face-decls/style:font-face");
    xmlNodeSetPtr pXmlNodes = pXPath->nodesetval;
    int nNodeCount = xmlXPathNodeSetGetLength(pXmlNodes);
    std::vector<XmlFont> aXMLFonts;
    std::vector<XmlFont> aSortedFonts;
    for (int i = 0; i < nNodeCount; ++i)
    {
        xmlNodePtr pXmlNode = pXmlNodes->nodeTab[i];
        xmlChar* pName = xmlGetProp(pXmlNode, BAD_CAST("name"));
        OString aName(reinterpret_cast<char const*>(pName));

        // Ignore numbers at the end, those are just appended to make all names unique.
        while (rtl::isAsciiDigit(static_cast<sal_uInt32>(aName[aName.getLength() - 1])))
        {
            aName = aName.copy(0, aName.getLength() - 1);
        }

        xmlChar* pFontFamilyGeneric = xmlGetProp(pXmlNode, BAD_CAST("font-family-generic"));
        OString aFontFamilyGeneric;
        if (pFontFamilyGeneric)
        {
            aFontFamilyGeneric = OString(reinterpret_cast<char const*>(pFontFamilyGeneric));
        }

        aXMLFonts.push_back(XmlFont{ aName, aFontFamilyGeneric });
        aSortedFonts.push_back(XmlFont{ aName, aFontFamilyGeneric });
        xmlFree(pName);
    }
    std::sort(aSortedFonts.begin(), aSortedFonts.end());
    size_t nIndex = 0;
    for (const auto& rFont : aSortedFonts)
    {
        // Without the accompanying fix in place, this test would have failed with:
        // - Expected: Liberation Sans
        // - Actual  : Lucida Sans1
        // i.e. the output was not lexicographically sorted, "u" was before "i".
        CPPUNIT_ASSERT_EQUAL(rFont.aName, aXMLFonts[nIndex].aName);
        // Without the accompanying fix in place, this test would have failed with:
        // - Expected: swiss
        // - Actual  : system
        // i.e. the output was not lexicographically sorted when style:name was the same, but
        // style:font-family-generic was not the same.
        CPPUNIT_ASSERT_EQUAL(rFont.aFontFamilyGeneric, aXMLFonts[nIndex].aFontFamilyGeneric);
        ++nIndex;
    }
    xmlXPathFreeObject(pXPath);
}

CPPUNIT_TEST_FIXTURE(XmloffStyleTest, testRtlGutter)
{
    // Given a document with a gutter margin and an RTL writing mode:
    // When loading that document from ODF:
    load(u"rtl-gutter.fodt");

    // Then make sure the page style's RtlGutter property is true.
    uno::Reference<style::XStyleFamiliesSupplier> xStyleFamiliesSupplier(getComponent(),
                                                                         uno::UNO_QUERY);
    uno::Reference<container::XNameAccess> xStyleFamilies
        = xStyleFamiliesSupplier->getStyleFamilies();
    uno::Reference<container::XNameAccess> xStyleFamily(xStyleFamilies->getByName("PageStyles"),
                                                        uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xStandard(xStyleFamily->getByName("Standard"),
                                                  uno::UNO_QUERY);
    bool bRtlGutter{};
    xStandard->getPropertyValue("RtlGutter") >>= bRtlGutter;
    // Without the accompanying fix in place, this test would have failed as
    // <style:page-layout-properties>'s style:writing-mode="..." did not affect RtlGutter.
    CPPUNIT_ASSERT(bRtlGutter);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
