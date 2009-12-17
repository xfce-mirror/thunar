<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                exclude-result-prefixes="#default">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl"/>

<!-- Use stylesheet -->
<xsl:param name="html.stylesheet" select="'../thunar.css'"/>

<!-- labels and numbering -->
<xsl:param name="autotoc.label.separator" select="'. '"/>
<xsl:param name="chapter.autolabel" select="1"/>

<!-- Don't force the use of index.html as root filename -->
<xsl:param name="root.filename" select="''"/>

<!--  Use element id (if present) as file name  -->
<xsl:variable name="use.id.as.filename">1</xsl:variable>

<xsl:template match="releaseinfo" mode="titlepage.mode">
  <span class="{name(.)}">
    <br/>
    <xsl:apply-templates mode="titlepage.mode"/>
    <br/>
  </span>
</xsl:template>

<!-- Use graphics in admonitions (note, warning, etc)  -->
<xsl:variable name="admon.graphics">0</xsl:variable>

<xsl:param name="admon.style">
	<xsl:text>text-align: left;</xsl:text></xsl:param>

<xsl:variable name="admon.graphics.path">stylesheet-images/</xsl:variable>

<xsl:variable name="admon.graphics.extension">.gif</xsl:variable>

<xsl:param name="table.border.thickness" select="'0.2pt'"/>

<xsl:param name="graphic.default.extension" select="png"/>

<!-- This requires an adapted template for tgroup (see end of stylesheet) -->
<xsl:attribute-set name="table.style">
	<xsl:attribute name="bgcolor">#fdf9f8</xsl:attribute>
	<xsl:attribute name="cellspacing">0</xsl:attribute>
	<xsl:attribute name="cellpadding">4</xsl:attribute>
</xsl:attribute-set>


<xsl:param name="generate.legalnotice.link" select="0"/>

<!-- set font styles for various tags   -->
<xsl:template match="guibutton">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guiicon">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guilabel">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenu">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenuitem">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guisubmenu">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="application">
<xsl:call-template name="inline.boldmonoseq"/>
</xsl:template>

<xsl:template match="caption">
<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<!-- Adapted template for tgroup. The only change is the addition of -->
<!-- table.style attributes -->
<xsl:template match="tgroup">
  <table xsl:use-attribute-sets="table.style">
    <xsl:choose>
      <!-- If there's a <?dbhtml table-summary="foo"?> PI, use it for
           the HTML table summary attribute -->
      <xsl:when test="processing-instruction('dbhtml')">
        <xsl:variable name="summary">
          <xsl:call-template name="dbhtml-attribute">
            <xsl:with-param name="pis"
                            select="processing-instruction('dbhtml')[1]"/>
            <xsl:with-param name="attribute" select="'table-summary'"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:if test="$summary != ''">
          <xsl:attribute name="summary">
            <xsl:value-of select="$summary"/>
          </xsl:attribute>
        </xsl:if>
      </xsl:when>
      <!-- Otherwise, if there's a title, use that -->
      <xsl:when test="../title">
        <xsl:attribute name="summary">
          <xsl:value-of select="string(../title)"/>
        </xsl:attribute>
      </xsl:when>
      <!-- Otherwise, forget the whole idea -->
      <xsl:otherwise><!-- nevermind --></xsl:otherwise>
    </xsl:choose>

    <xsl:if test="../@pgwide=1">
      <xsl:attribute name="width">100%</xsl:attribute>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="../@frame='none'">
        <xsl:attribute name="border">0</xsl:attribute>
      </xsl:when>
      <xsl:when test="$table.borders.with.css != 0">
        <xsl:attribute name="border">0</xsl:attribute>
        <xsl:choose>
          <xsl:when test="../@frame='topbot' or ../@frame='top'">
            <xsl:attribute name="style">
              <xsl:call-template name="border">
                <xsl:with-param name="side" select="'top'"/>
              </xsl:call-template>
            </xsl:attribute>
          </xsl:when>
          <xsl:when test="../@frame='sides'">
            <xsl:attribute name="style">
              <xsl:call-template name="border">
                <xsl:with-param name="side" select="'left'"/>
              </xsl:call-template>
              <xsl:call-template name="border">
                <xsl:with-param name="side" select="'right'"/>
              </xsl:call-template>
            </xsl:attribute>
          </xsl:when>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:attribute name="border">1</xsl:attribute>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:variable name="colgroup">
      <colgroup>
        <xsl:call-template name="generate.colgroup">
          <xsl:with-param name="cols" select="@cols"/>
        </xsl:call-template>
      </colgroup>
    </xsl:variable>

    <xsl:variable name="explicit.table.width">
      <xsl:call-template name="dbhtml-attribute">
        <xsl:with-param name="pis"
                        select="../processing-instruction('dbhtml')[1]"/>
        <xsl:with-param name="attribute" select="'table-width'"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="table.width">
      <xsl:choose>
        <xsl:when test="$explicit.table.width != ''">
          <xsl:value-of select="$explicit.table.width"/>
        </xsl:when>
        <xsl:when test="$default.table.width = ''">
          <xsl:text>100%</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$default.table.width"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="$default.table.width != ''
                  or $explicit.table.width != ''">
      <xsl:attribute name="width">
        <xsl:choose>
          <xsl:when test="contains($table.width, '%')">
            <xsl:value-of select="$table.width"/>
          </xsl:when>
          <xsl:when test="$use.extensions != 0
                          and $tablecolumns.extension != 0">
            <xsl:choose>
              <xsl:when test="function-available('stbl:convertLength')">
                <xsl:value-of select="stbl:convertLength($table.width)"/>
              </xsl:when>
              <xsl:when test="function-available('xtbl:convertLength')">
                <xsl:value-of select="xtbl:convertLength($table.width)"/>
              </xsl:when>
              <xsl:otherwise>
                <xsl:message terminate="yes">
                  <xsl:text>No convertLength function available.</xsl:text>
                </xsl:message>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$table.width"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="$use.extensions != 0
                      and $tablecolumns.extension != 0">
        <xsl:choose>
          <xsl:when test="function-available('stbl:adjustColumnWidths')">
            <xsl:copy-of select="stbl:adjustColumnWidths($colgroup)"/>
          </xsl:when>
          <xsl:when test="function-available('xtbl:adjustColumnWidths')">
            <xsl:copy-of select="xtbl:adjustColumnWidths($colgroup)"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:text>No adjustColumnWidths function available.</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:copy-of select="$colgroup"/>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:apply-templates select="thead"/>
    <xsl:apply-templates select="tbody"/>
    <xsl:apply-templates select="tfoot"/>

    <xsl:if test=".//footnote">
      <tbody class="footnotes">
        <tr>
          <td colspan="{@cols}">
            <xsl:apply-templates select=".//footnote" 
                                 mode="table.footnote.mode"/>
          </td>
        </tr>
      </tbody>
    </xsl:if>
  </table>
</xsl:template>


</xsl:stylesheet>

