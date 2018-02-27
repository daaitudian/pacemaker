<!--
 Copyright 2018 Red Hat, Inc.
 Author: Jan Pokorny <jpokorny@redhat.com>
 Part of pacemaker project
 SPDX-License-Identifier: GPL-2.0-or-later
 -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:cibtr-2="http://clusterlabs.org/ns/pacemaker/cibtr-2">
<xsl:output method="text" encoding="UTF-8"/>

<xsl:variable name="NL" select="'&#xA;'"/>

<xsl:template name="MapMsg-2">
  <xsl:param name="Replacement"/>
  <xsl:choose>
    <xsl:when test="not($Replacement)"/>
    <xsl:when test="count($Replacement) != 1">
      <xsl:message terminate="yes">
        <xsl:value-of select="concat('INTERNAL ERROR:',
                                     $Replacement/../@msg-prefix,
                                     ': count($Replacement) != 1',
                                     ' does not hold (',
                                     count($Replacement), ')')"/>
      </xsl:message>
    </xsl:when>
    <xsl:otherwise>
      <cibtr-2:noop>
        <xsl:choose>
          <xsl:when test="string($Replacement/@with)">
            <xsl:value-of select="concat('renaming ', $Replacement/@what,
                                         ' as ', $Replacement/@with)"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="concat('dropping ', $Replacement/@what)"/>
          </xsl:otherwise>
        </xsl:choose>
      </cibtr-2:noop>
      <xsl:if test="$Replacement/@msg-extra">
        <cibtr-2:noop>
          <xsl:value-of select="concat($NL, '     ... ',
                                       $Replacement/@msg-extra)"/>
        </cibtr-2:noop>
      </xsl:if>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="cibtr-2:map">
  <xsl:value-of select="concat('Translation tables v2 in detail', $NL,
                               '===============================', $NL, $NL)"/>
  <xsl:apply-templates select="*"/>
</xsl:template>

<xsl:template match="cibtr-2:table">
  <xsl:value-of select="concat('Details for the ', @for, ' table:', $NL)"/>
  <xsl:value-of select="concat(string(preceding-sibling::comment()[1]), $NL)"/>
  <xsl:apply-templates select="*"/>
  <xsl:value-of select="$NL"/>
</xsl:template>

<xsl:template match="cibtr-2:replace">
  <xsl:value-of select="'   - '"/>
  <xsl:call-template name="MapMsg-2">
    <xsl:with-param name="Replacement" select="."/>
  </xsl:call-template>
  <xsl:value-of select="$NL"/>
</xsl:template>

<xsl:template match="*">
  <xsl:apply-templates select="*"/>
</xsl:template>

</xsl:stylesheet>
