
var attention = function(labelsA, labelsB, data) {
  console.log(data)
  console.log(labelsA);
  console.log(labelsB);
  var linkData = [];
  data.forEach(function(a, ai) {
    a.forEach(function(b, bi) {
      linkData.push({ai: +ai, bi: +bi, v: +data[ai][bi]});
    });
  });

  var nodeMargin = 10;
  var networkMargin = 120;

  var html = d3.select('#attentional-ex2');
  var outerWidth = 984;   // html.node().getBoundingClientRect().width;
  var outerHeight = 240;  // html.node().getBoundingClientRect().height;
  var margin = {top: 36, right: nodeMargin, bottom: 24, left: nodeMargin};
  var width = outerWidth - margin.left - margin.right;
  var height = outerHeight - margin.top - margin.bottom;

  var xa = d3.scaleBand().domain(d3.range(labelsA.length)).paddingInner(0.5).range([nodeMargin, width - nodeMargin]);

  var xb = d3.scaleBand().domain(d3.range(labelsB.length)).paddingInner(0.5).range([nodeMargin, width - nodeMargin]);

  var nodeSizeA = xa.bandwidth();
  var nodeSizeB = xb.bandwidth();

  var networkHeightA = nodeSizeA + nodeMargin * 2;
  var networkHeightB = nodeSizeB + nodeMargin * 2;

  var link = function(d) {
    // console.log(nodeSizeB, nodeSizeA, nodeMargin, d.bi, xb(d.bi))
    var p = 'M' + (xb(d.bi) + nodeSizeB / 2) + ',' + (nodeSizeB + nodeMargin) + 'C' + (xb(d.bi) + nodeSizeB / 2) + ',' +
        (nodeSizeB + networkMargin / 3 + nodeMargin) + ' ' + (xa(d.ai) + nodeSizeA / 2) + ',' +
        (networkMargin - networkMargin / 3 + nodeMargin) + ' ' + (xa(d.ai) + nodeSizeA / 2) + ',' +
        (networkMargin + nodeMargin);
    // console.log(p);
    return p;
  };

  html.select('svg .labels').attr('transform', 'translate(' + (width + nodeMargin * 3) + ', 0)');

  html.select('svg').selectAll('*').remove();

  var svg = html.select('svg')
                .attr('viewBox', '0 0 ' + outerWidth + ' ' + outerHeight)
                .append('g')
                .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')');

  svg.selectAll('*').remove();


  var networkA = svg.append('g').attr('transform', 'translate(0, ' + networkMargin + ')');

  networkA.append('rect')
      .attr('class', 'background')
      .attr('width', width)
      .attr('height', nodeSizeA + 2 * nodeMargin)
      .attr('rx', nodeMargin * 0.7)
      .attr('ry', nodeMargin * 0.7);

  var cellA = networkA.selectAll('.cell')
                  .data(labelsA)
                  .enter()
                  .append('g')
                  .attr('class', 'cell')
                  .attr(
                      'transform',
                      function(d, i) {
                        return 'translate(' + xa(i) + ', ' + nodeMargin + ')';
                      })
                  .on('mouseover',
                      function(d, i) {
                        link.style('display', function(ld) {
                          return ld.ai === i ? '' : 'none';
                        });
                      })
                  .on('mouseout', function() {
                    link.style('display', '');
                  });

  cellA.append('rect')
      .attr('class', 'shadow')
      .attr('transform', 'translate(1, 3)')
      .attr('width', nodeSizeA)
      .attr('height', nodeSizeA)
      .attr('rx', 4)
      .attr('ry', 4);

  cellA.append('rect')
      .attr('class', 'node')
      .attr('width', nodeSizeA)
      .attr('height', nodeSizeA)
      .attr('rx', 4)
      .attr('ry', 4);

  cellA.append('line')
      .attr('class', 'arrow')
      .attr('marker-start', 'url(#edgeArrowheadLeft)')
      .attr('marker-end', 'url(#edgeArrowhead)')
      .attr('x1', nodeSizeA + 8)
      .attr('x2', nodeSizeA + nodeMargin + 20)
      .attr('y1', nodeSizeA / 2)
      .attr('y2', nodeSizeA / 2);

  cellA.append('line')
      .attr('class', 'arrow')
      .attr('marker-end', 'url(#edgeArrowhead)')
      .attr('transform', 'translate(' + nodeSizeA / 2 + ', 0)')
      .attr('y1', nodeSizeA + nodeMargin * 2 - 2)
      .attr('y2', nodeSizeA + 4);

  cellA.append('text').style('opacity', 0.8).text('A').attr('dx', nodeSizeA / 2).attr('dy', nodeSizeA / 2 + 1);

  cellA.append('text')
      .attr('class', 'label')
      .attr('dx', nodeSizeA / 2)
      .attr('dy', nodeSizeA + 24 + 8)
      .text(function(d) {
        return d;
      });

  cellA.append('rect')
      .style('opacity', 0)
      .attr(
          'transform',
          'translate(-' + ((xa.step() - nodeSizeA) / 2) + ',-' + (nodeMargin + (networkMargin - networkHeightB) / 2) +
              ')')
      .attr('width', xa.step())
      .attr('height', networkHeightA + (networkMargin - networkHeightA) / 2);

  //
  // Network B
  //

  var networkB = svg.append('g');

  networkB.append('rect')
      .attr('class', 'background')
      .attr('width', width)
      .attr('height', nodeSizeB + 2 * nodeMargin)
      .attr('rx', nodeMargin * 0.7)
      .attr('ry', nodeMargin * 0.7);

  var cellB = networkB.selectAll('.cell')
                  .data(labelsB)
                  .enter()
                  .append('g')
                  .attr('class', 'cell')
                  .attr(
                      'transform',
                      function(d, i) {
                        return 'translate(' + xb(i) + ',' + nodeMargin + ')';
                      })
                  .on('mouseover',
                      function(d, i) {
                        link.style('display', function(ld) {
                          return ld.bi === i ? '' : 'none';
                        });
                      })
                  .on('mouseout', function() {
                    link.style('display', '');
                  });

  cellB.append('rect')
      .attr('class', 'shadow')
      .attr('transform', 'translate(1, 3)')
      .attr('width', nodeSizeB)
      .attr('height', nodeSizeB)
      .attr('rx', 4)
      .attr('ry', 4);

  cellB.append('rect')
      .attr('class', 'node')
      .attr('width', nodeSizeB)
      .attr('height', nodeSizeB)
      .attr('rx', 4)
      .attr('ry', 4);

  cellB.append('line')
      .attr('class', 'arrow')
      .attr('marker-end', 'url(#edgeArrowhead)')
      .attr('x1', nodeSizeB)
      .attr('x2', nodeSizeB + nodeMargin + 14)
      .attr('y1', nodeSizeB / 2)
      .attr('y2', nodeSizeB / 2);

  cellB.append('line')
      .attr('class', 'arrow')
      .attr('marker-end', 'url(#edgeArrowhead)')
      .attr('transform', 'translate(' + nodeSizeB / 2 + ', 0)')
      .attr('y1', 0)
      .attr('y2', -nodeMargin * 2 + 5);

  cellB.append('text').style('opacity', 0.8).text('B').attr('dx', nodeSizeB / 2).attr('dy', nodeSizeB / 2 + 1);

  cellB.append('text')
      .attr('class', 'label')
      .attr(
          'dx',
          function(d) {
            return d === 'européenne' ? nodeSizeB / 2 + 15 : nodeSizeB / 2;
          })
      .attr('dy', -24 - 5)
      .text(function(d) {
        return d;
      });

  cellB.append('rect')
      .style('opacity', 0)
      .attr('transform', 'translate(-' + ((xb.step() - nodeSizeB) / 2) + ',-' + nodeMargin + ')')
      .attr('width', xb.step())
      .attr('height', networkHeightB + (networkMargin - networkHeightB) / 2);
  //
  // Links
  //
  var linkGroup = svg.append('g').attr('class', '.link-group')

  var link = linkGroup.selectAll('.link')
                 .data(linkData)
                 .enter()
                 .append('path')
                 .attr('class', 'link')
                 .attr('d', link)
                 .style('opacity', function(d) {
                   return d.v;
                 });
  //
  //
  //
  // var hoverB = svg.selectAll(".hover-b")
  //     .data(labelsB)
  //   .enter().append("rect")
  //     .attr("class", "hover-b")
  //     .style("opacity", 0)
  //     .attr("transform", function(d, i) { return "translate(" + (xb(i) - xb.step() / 4) + ", 0)"; })
  //     .attr("width", xb.step())
  //     .attr("height", networkMargin / 2 + nodeSizeB)
};

var labelsA = [
  'the', 'agreement', 'on', 'the', 'European', 'Economic', 'Area', 'was', 'signed', 'in', 'August', '1992', '.', '<end>'
];

var labelsB = [
  'l’', 'accord', 'sur', 'la', 'zone', 'économique', 'européenne', 'a', 'été', 'signé', 'en', 'août', '1992', '.',
  '<end>'
];

var data = d3.csvParseRows(d3.select('#attentional-ex2-data').text());
attention(labelsA, labelsB, data);
