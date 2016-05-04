<?xml version="1.0"?>
<!DOCTYPE doc [
  <!ENTITY nl "
">
]>

<!-- From https://groups.google.com/forum/#!topic/protobuf/resX1vz3U5Q -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xsd="http://www.w3.org/2001/XMLSchema" version="1.0">
  <!-- exec with: xsltproc xsd2proto.xsl c++-api-xml/compound.xsd -->

  <xsl:output method="text"/>
  <xsl:strip-space elements="*"/>

  <xsl:template match="xsd:schema">
    <xsl:text>syntax = "proto2";&nl;&nl;</xsl:text>
    <xsl:text>package doxygen;&nl;&nl;</xsl:text>
    <xsl:apply-templates match="*"/>
  </xsl:template>

  <xsl:template match="xsd:complexType|xsd:group">
    <xsl:text>message </xsl:text>
    <xsl:call-template name="typename"><xsl:with-param name="type" select="@name"/></xsl:call-template>
    <xsl:text> {&nl;</xsl:text>

    <xsl:if test="(@mixed = 'true' or name(.) = 'xsd:group') and count(./xsd:choice/xsd:group|./xsd:sequence/xsd:group|./xsd:group) = 0">
      <xsl:text>  optional string text = 1;&nl;</xsl:text>
    </xsl:if>

    <xsl:text>  // Attributes&nl;</xsl:text>
    <xsl:apply-templates select="./xsd:attribute" mode="attributes"/>

    <xsl:for-each select="./xsd:simpleContent/xsd:extension">
      <xsl:apply-templates
        select="./xsd:attribute"
        mode="attributes"/>
    </xsl:for-each>

    <xsl:text>  // Elements&nl;</xsl:text>
    <xsl:if test="count(./xsd:choice) > 0">
      <xsl:text>  // (Only one will be filled in.)&nl;</xsl:text>
    </xsl:if>

    <xsl:apply-templates select="./xsd:choice/xsd:group|./xsd:sequence/xsd:group|./xsd:group" mode="group"/>
    <xsl:apply-templates select="./xsd:choice/xsd:element" mode="elements"/>
    <xsl:apply-templates select="./xsd:sequence/xsd:element" mode="elements"/>

    <xsl:text>}&nl;&nl;</xsl:text>
  </xsl:template>

  <xsl:template name="typename">
    <xsl:choose>
      <xsl:when test="$type = 'xsd:string'">
        <xsl:text>string</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'xsd:integer'">
        <xsl:text>int32</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'DoxBool'">
        <xsl:text>bool</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'DoxVersionNumber'">
        <xsl:text>string</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'DoxCharRange'">
        <xsl:text>string</xsl:text>
      </xsl:when>
      <xsl:when test="contains($type, 'Type')">
        <xsl:value-of select="concat(translate(substring($type, 1, 1), 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'), substring(substring-before($type, 'Type'), 2))"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="concat(translate(substring($type, 1, 1), 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'), substring($type, 2))"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="xsd:attribute" mode="attributes">
    <xsl:choose>
      <xsl:when test="@use = 'required'">
        <xsl:text>  required </xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>  optional </xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:call-template name="typename"><xsl:with-param name="type" select="@type"/></xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> = </xsl:text>
    <xsl:value-of select="position() + 1"/>
    <xsl:text>;&nl;</xsl:text>
  </xsl:template>

  <xsl:template match="xsd:element" mode="elements">
    <xsl:choose>
      <xsl:when test="@maxOccurs = 'unbounded' or ../@maxOccurs = 'unbounded'">
        <xsl:text>  repeated </xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>  optional </xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
      <xsl:when test="count(@type) > 0">
        <xsl:call-template name="typename"><xsl:with-param name="type" select="@type"/></xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>string</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> = </xsl:text>
    <xsl:value-of select="position() + count(../../xsd:attribute) + 1"/>
    <xsl:text>;&nl;</xsl:text>
  </xsl:template>

  <xsl:template match="xsd:group" mode="group">
    <xsl:choose>
      <xsl:when test="@maxOccurs = 'unbounded'">
        <xsl:text>  repeated </xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>  optional </xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:call-template name="typename"><xsl:with-param name="type" select="@ref"/></xsl:call-template>
    <xsl:choose>
      <xsl:when test="@maxOccurs = 'unbounded'">
        <xsl:text> element = 1;&nl;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text> more = 1;&nl;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="xsd:simpleType/xsd:restriction">
    <xsl:if test="(../@name) != 'DoxBool' and
                  (../@name) != 'DoxVersionNumber' and
                  (../@name) != 'DoxCharRange'">
      <xsl:text>enum </xsl:text>
      <xsl:call-template name="typename">
        <xsl:with-param name="type" select="../@name"/>
      </xsl:call-template>
      <xsl:text> {&nl;</xsl:text>

      <xsl:apply-templates select="./xsd:enumeration" mode="enumvalues">
        <xsl:with-param name="type" select="../@name"/>
      </xsl:apply-templates>

      <xsl:text>}&nl;&nl;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="xsd:enumeration" mode="enumvalues">
    <xsl:text>  </xsl:text>
    <xsl:value-of select="translate($type, 'abcdefghijklmnopqrstuvwxyz-', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_')"/>
    <xsl:text>_</xsl:text>
    <xsl:value-of select="translate(@value, 'abcdefghijklmnopqrstuvwxyz-', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_')"/>
    <xsl:text> = </xsl:text>
    <xsl:value-of select="position()"/>
    <xsl:text>;&nl;</xsl:text>
  </xsl:template>

</xsl:stylesheet>
