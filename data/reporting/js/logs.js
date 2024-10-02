document.addEventListener("DOMContentLoaded", function () {
    loadScripts().then(function() {
        insertNodes();
    }).catch(function(error) {
        console.log(error);
    });
});

function loadScripts() {
    const scripts = ['data/report.js', 'data/nodes.js'];
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

function insertNodes() {
    const target = document.getElementById("messageTarget");
    temp = document.getElementsByTagName("template")[0];
    msgContent = temp.content;
    
    
    for (i = 0; i < logMessages.length; i++) {
        let msg = logMessages[i];
    
        let loopFlag = loopDetector(msg.message_parts);
        // this should be removed later
        if (loopFlag) {
            console.log("found signal loop related error message.");
        }
        
        temp.content.querySelector('#parts').textContent = '';      // this isn't pretty but it's the first solution that worked :)
        temp.content.querySelector('#msgNumber').textContent = "Message " + (i+1) + ":";
    
        temp.content.querySelector('#sev').textContent = msg.severity;
        switch(msg.severity) {
            case "LOG_INFO":
                temp.content.querySelector('#sev').style.color = '#0DCAF0';
                break;
            case "LOG_WARNING":
                temp.content.querySelector('#sev').style.color = '#FFC107';
                break;
            case "LOG_ERROR":
                temp.content.querySelector('#sev').style.color = '#BB2D3B';
                break;
            default:
                temp.content.querySelector('#sev').style.color = 'black';
        }
    
        temp.content.querySelector('#sev').textContent = msg.severity;
        temp.content.querySelector('#src').textContent = msg.source;
        temp.content.querySelector('#parts').appendChild(getPartsAsHtml(msg.message_parts, loopFlag));
        templateContent = document.importNode(msgContent, true);
        target.append(templateContent);
    }
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

function loopDetector(msgParts) {
    const triggerString = "signal loop";
    let found = false;
    msgParts.forEach(part => {
        for(const key in part) {
            if (part[key] === 'string') {
                if (part.data.includes(triggerString)) {
                    found = true;
                }
            }
        }
    })
    return found;
}

function getPartsAsHtml(msgParts, isLoop) {
    const container = document.createElement('div');
    msgParts.forEach(part => {
        if (Object.hasOwn(part, 'id')) {
            logFillOffcanvas(part.id);
        }
        for (const key in part) {
            if (part[key] === 'string') {
                const textNode = document.createTextNode(part.data + ' ');
                container.append(textNode);
            } else if (part[key] === 'node') {
                const link = document.createElement('a');
                link.textContent = 'NODE(id: ' + part.id + ') ';
                link.href = `#log${part.id}`;
                link.classList.add('custom-link');
                //link.classList.add('btn');
                link.setAttribute('data-bs-toggle', 'offcanvas');
                link.setAttribute('role', 'button');
                link.setAttribute('aria-controls', 'offcanvasLogMessages');
                
                container.append(link);
            }
        }
    })
    if (isLoop) {
        const link = document.createElement('a');
        link.textContent = 'Show as graph';
        link.href = `gatery_loops.html`;
        link.classList.add('custom-link-2');

        container.append(link);
    }
    return container;
}

function logFillOffcanvas(nodeId) {
    const target = document.getElementById("offcanvasTarget");
    offcanvas = document.getElementsByTagName("template")[1];
    offcanvasContent = offcanvas.content;
    
    offcanvas.content.querySelector('div').id = `log${nodeId}`;
    
    offcanvas.content.querySelector("h5").textContent = "Whole lotta *pretty* data (I know, it's still ugly, but it used to be worse!)";
    node = getNodeById(nodeId);

    if (node === undefined) {
        offcanvas.content.getElementById('undefined').innerHTML = `<p>This node has been removed during post processing</p>`;
    } else {
        offcanvas.content.getElementById('undefined').innerHTML = "";
        offcanvas.content.getElementById('id').innerHTML = `Node ID: ${node.id}`;
        offcanvas.content.getElementById('name').innerHTML = getNodeProperty(node, 'name', 'Name');
        offcanvas.content.getElementById('group').innerHTML = `Group: ${node.group}`;
        offcanvas.content.getElementById('stacktrace').innerHTML = getStacktrace(node["stack_trace"]);
        offcanvas.content.getElementById('type').innerHTML = `Type: ${node.type}`;
        offcanvas.content.getElementById('meta').innerHTML = getNodeProperty(node, 'meta', 'Meta information');
        offcanvas.content.getElementById('clocks').innerHTML = getNodeProperty(node, 'clocks', 'Clocks');
        offcanvas.content.getElementById('inputPorts').innerHTML = getNodeProperty(node, 'inputPorts', 'Input ports');
        offcanvas.content.getElementById('outputPorts').innerHTML = getNodeProperty(node, 'outputPorts', 'Output ports');
    }
    
    logMessageOffcanvas = document.importNode(offcanvasContent, true);
    target.append(logMessageOffcanvas);
    
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

function getNodeById(nodeId) {
    let wanted;
    hierarchyNodeData.forEach(node => {
        if (node.id == nodeId) {
            wanted = node;
        }
    })
    return wanted;
}

// seems to be deprecated...
function processEntity(entity, showStackTrace) {
    let listHTML = "</ul>";
    if (entity === undefined) {
        listHTML += "<li>This node has been removed during post processing.</li>";
    } else {
        listHTML += getObject(entity, listHTML, showStackTrace);
    }
    listHTML += "</ul>";
    return listHTML;
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
