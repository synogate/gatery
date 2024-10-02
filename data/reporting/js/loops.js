document.addEventListener("DOMContentLoaded", function () {
    loadScripts().then(function() {
        insertSVG();
    }).catch(function(error) {
        console.log(error);
    });
});

function loadScripts() {
    const scripts = ["data/report.js", "data/nodes.js", "data/prerenderedSubnets.js"];
    const promises = scripts.map(function(src) {
        return new Promise(function(resolve, reject) {
            var script = document.createElement('script');
            script.src = src;
            script.onload = resolve;
            script.onerror = function () {
                fileNotAvailable(src);
                console.log("Failed to load " + src);
                reject(new Error("Failed to load script " + src));
            };
            document.head.append(script);
        });
    });
    return Promise.all(promises);
}

function fileNotAvailable(file) {
    const target = document.getElementById("errorMessageTarget");
    const error = document.createElement("div");
    error.classList.add("alert");
    error.classList.add("alert-danger");
    error.setAttribute("role", "alert");
    error.textContent = "File " + file + " does not exist or cannot be found.";
    target.append(error);
}


function insertSVG() {
    const target = document.getElementById("svgTarget");
    temp = document.getElementsByTagName("template")[0];
    for (i = 0; i < prerenderedSubnets.length; i++) {
        let prerenderedSubnet = prerenderedSubnets[i];
        temp.content.querySelector('#svg-holder').innerHTML = prerenderedSubnet.content;
        templateContent = document.importNode(temp.content, true);
        target.append(templateContent);

        makeInteractive(prerenderedSubnet.imageId);
    }
}

function makeInteractive(index) {
    let objectId = 'svg-object-' + index;
    var svgObject = document.getElementById(objectId);
    if (svgObject) {
        svgObject.addEventListener("load", function () {
            var svgDocument = svgObject;
            if (svgDocument) {
                nodeClicker(svgDocument, index);

            } else {
                console.log("Content document not available.");
            }
        });
    } else {
        console.log("SVG object element not found.");
    }
}

function nodeClicker(svgDocument, index) {
    let nodes = svgDocument.getElementsByClassName("node");
    let i;
    for (i = 0; i < nodes.length; i++) {
        let node = nodes[i];
        if (node) {
            let nodeId = node.getAttribute("id");
            loopFillOffcanvas(nodeId);
            
            node.addEventListener("click", function () {
                console.log(nodeId);
                let offcanvasId = `#loop${nodeId}`;
                $(offcanvasId).offcanvas('show');
            });
        } else {
            let errorMessage = "Node not found.";
            console.log(errorMessage);
        }
    }
}


function loopFillOffcanvas(nodeId) {
    const target = document.getElementById("offcanvasTarget");
    offcanvas = document.getElementsByTagName("template")[1];
    offcanvasContent = offcanvas.content;
    
    offcanvasContent.querySelector('div').id = `loop${nodeId}`;
    
    offcanvas.content.querySelector("h5").textContent = "Whole lotta *pretty* data (I know, it's still ugly, but it used to be worse!)";
    node = getNodeById(nodeId);

    if (node === undefined) {
        offcanvas.content.getElementById('undefined').innerHTML = `<p>This node has been removed during post processing</p>`;
    } else {
        // TODO: offcanvas.content can probably be optimized...
        offcanvasContent.getElementById('undefined').innerHTML = "";
        offcanvasContent.getElementById('id').innerHTML = `Node ID: ${node.id}`;
        offcanvasContent.getElementById('name').innerHTML = getNodeProperty(node, 'name', 'Name');
        offcanvasContent.getElementById('group').innerHTML = `Group: ${node.group}`;
        offcanvasContent.getElementById('stacktrace').innerHTML = getStacktrace(node["stack_trace"]);
        offcanvasContent.getElementById('type').innerHTML = `Type: ${node.type}`;
        offcanvasContent.getElementById('meta').innerHTML = getNodeProperty(node, 'meta', 'Meta information');
        offcanvasContent.getElementById('clocks').innerHTML = getNodeProperty(node, 'clocks', 'Clocks');
        offcanvasContent.getElementById('inputPorts').innerHTML = getNodeProperty(node, 'inputPorts', 'Input ports');
        offcanvasContent.getElementById('outputPorts').innerHTML = getNodeProperty(node, 'outputPorts', 'Output ports');
    }

    loopCircuitOffcanvas = document.importNode(offcanvasContent, true);
    target.append(loopCircuitOffcanvas);
}

function getNodeById(nodeId) {
    let wanted;
    hierarchyNodeData.forEach(node => {
        if (node.id == nodeId) {
            wanted = node;
        }
    })
    return wanted;
}

function getStacktrace(stacktrace) {
    let stacktraceHTML = `<div class="">`;
    if (stacktrace.length === 0) {
        stacktraceHTML += `Stacktrace: No stacktrace found for this node.`;
    } else {
        stacktraceHTML += `<button class="custom-btn btn-sm dropdown-toggle" type="button" data-bs-toggle="dropdown" aria-expanded="false">Stack Trace</button>`;
        stacktraceHTML += `<ul class="dropdown-menu dropdown-menu-end" style="background-color: #FFFFFF;">`;
        stacktrace.forEach(frame => {
            let location = "vscode://file/Users/lucalichterman/Documents/Synogate/gatery/source/gatery/debug/reporting/ReportInterface.h";
            let lineNumber = frame.lineNumber;
            let frameName = frame.frameName;
            stacktraceHTML += `<li><a class="dropdown-item" href="${location}:${lineNumber}">${frameName}</a></li>`;
        })
    }
    stacktraceHTML += `</ul></div>`;
    return stacktraceHTML;
}

function getNodeProperty(node, propertyName, description) {
    if (!(propertyName in node) || node[propertyName] === "" || node[propertyName].length === 0) {
        return `${description}: No ${description} found for this node.`;
    } else if (Array.isArray(node[propertyName]) || typeof node[propertyName] === 'object') {
        return `${description}:\n<ul>${getObject(node[propertyName], "", false)}</ul>`;
    }
    return `${description}: ${node[propertyName]}`;
}

function getObject(object, html, showStackTrace) {
    for (const key in object) {
        if (Object.hasOwn(object, key)) {
            const value = object[key];
            if (Array.isArray(value)) {
                html += `<li><strong>${key}:</strong><ul>`;
                if (value.length == 0) {
                    html += `<li>Empty</li>`;
                } else if (key == "stack_trace") {
                    html += getStacktrace(value);
                } else {
                    for (const item of value) {
                        if (typeof item === "object" && item !== null) {
                            let innerHtml = '';
                            html += getObject(item, innerHtml);
                        } else {
                            html += `<li>${item}</li>`;
                        }
                    }
                }
                html += `</ul></li>`;
            } else {
                html += `<li><strong>${key}:</strong>`;
                if (typeof value === "object" && value !== null) {
                    let innerHtml = `<ul>`;
                    html += getObject(value, innerHtml);
                    html += `</ul>`;
                } else {
                    html += ` ${value}</li>`;
                }
            }
        }
    }
    return html;
}
